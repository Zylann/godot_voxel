#include "voxel_thread_pool.h"

#include <core/os/os.h>
#include <core/os/semaphore.h>
#include <core/os/thread.h>

VoxelThreadPool::VoxelThreadPool() {
	_tasks_mutex = Mutex::create();
	_tasks_semaphore = Semaphore::create();
	_completed_tasks_mutex = Mutex::create();
}

VoxelThreadPool::~VoxelThreadPool() {
	for (size_t i = 0; i < _thread_count; ++i) {
		ThreadData &d = _threads[i];
		destroy_thread(d);
	}

	if (_completed_tasks.size() != 0) {
		// We don't have ownership over tasks, so it's an error to destroy the pool without handling them
		ERR_PRINT("There are unhandled completed tasks remaining!");
	}

	memdelete(_tasks_mutex);
	memdelete(_tasks_semaphore);
	memdelete(_completed_tasks_mutex);
}

void VoxelThreadPool::create_thread(ThreadData &d, uint32_t i) {
	d.pool = this;
	d.stop = false;
	d.waiting = false;
	d.index = i;
	d.thread = Thread::create(thread_func_static, &d);
}

void VoxelThreadPool::destroy_thread(ThreadData &d) {
	d.stop = true;
	_tasks_semaphore->post();
	Thread::wait_to_finish(d.thread);
	memdelete(d.thread);
	d = ThreadData();
}

void VoxelThreadPool::set_thread_count(uint32_t count) {
	if (count > MAX_THREADS) {
		count = MAX_THREADS;
	}
	for (uint32_t i = count; i < _thread_count; ++i) {
		ThreadData &d = _threads[i];
		destroy_thread(d);
	}
	for (uint32_t i = _thread_count; i < count; ++i) {
		ThreadData &d = _threads[i];
		create_thread(d, i);
	}
	_thread_count = count;
}

void VoxelThreadPool::set_batch_count(uint32_t count) {
	_batch_count = count;
}

void VoxelThreadPool::set_priority_update_period(uint32_t milliseconds) {
	_priority_update_period = milliseconds;
}

void VoxelThreadPool::enqueue(IVoxelTask *task) {
	CRASH_COND(task == nullptr);
	{
		MutexLock lock(_tasks_mutex);
		TaskItem t;
		t.task = task;
		_tasks.push_back(t);
	}
	// TODO Do I need to post a certain amount of times?
	_tasks_semaphore->post();
}

void VoxelThreadPool::thread_func_static(void *p_data) {
	ThreadData &data = *static_cast<ThreadData *>(p_data);
	VoxelThreadPool &pool = *data.pool;
	pool.thread_func(data);
}

void VoxelThreadPool::thread_func(ThreadData &data) {
	data.debug_state = STATE_RUNNING;

	std::vector<TaskItem> tasks;
	std::vector<IVoxelTask *> cancelled_tasks;

	while (!data.stop) {
		{
			data.debug_state = STATE_PICKING;
			const uint32_t now = OS::get_singleton()->get_ticks_msec();

			MutexLock lock(_tasks_mutex);

			// Pick best tasks
			for (uint32_t bi = 0; bi < _batch_count && _tasks.size() != 0; ++bi) {
				size_t best_index;
				int best_priority = 999999;

				for (size_t i = 0; i < _tasks.size(); ++i) {
					TaskItem &item = _tasks[i];

					if (now - item.last_priority_update_time > _priority_update_period) {
						if (item.task->is_cancelled()) {
							cancelled_tasks.push_back(item.task);
							continue;

						} else {
							item.cached_priority = item.task->get_priority();
						}
					}

					if (item.cached_priority < best_priority) {
						best_priority = item.cached_priority;
						best_index = i;
					}
				}

				tasks.push_back(_tasks[best_index]);
				_tasks[best_index] = _tasks.back();
				_tasks.pop_back();
			}
		}

		if (cancelled_tasks.size() > 0) {
			MutexLock lock(_completed_tasks_mutex);
			for (size_t i = 0; i < cancelled_tasks.size(); ++i) {
				_completed_tasks.push_back(cancelled_tasks[i]);
			}
			cancelled_tasks.clear();
		}

		if (tasks.empty()) {
			data.debug_state = STATE_WAITNG;

			// Wait for more tasks
			data.waiting = true;
			_tasks_semaphore->wait();
			data.waiting = false;

		} else {
			data.debug_state = STATE_RUNNING;

			for (size_t i = 0; i < tasks.size(); ++i) {
				TaskItem &item = tasks[i];
				if (!item.task->is_cancelled()) {
					VoxelTaskContext ctx;
					ctx.thread_index = data.index;
					item.task->run(ctx);
				}
			}
			{
				MutexLock lock(_completed_tasks_mutex);
				for (size_t i = 0; i < tasks.size(); ++i) {
					TaskItem &item = tasks[i];
					_completed_tasks.push_back(item.task);
				}
			}
		}
	}

	data.debug_state = STATE_STOPPED;
}

void VoxelThreadPool::wait_for_all_tasks() {
	const uint32_t suspicious_delay = 10;

	uint32_t before = OS::get_singleton()->get_ticks_msec();
	bool error1_reported = false;

	// Wait until all tasks have been taken
	while (true) {
		{
			MutexLock lock(_tasks_mutex);
			if (_tasks.size() == 0) {
				break;
			}
		}

		OS::get_singleton()->delay_usec(1000);

		if (!error1_reported && OS::get_singleton()->get_ticks_msec() - before > suspicious_delay) {
			ERR_PRINT("Waiting for all tasks to be picked is taking too long");
			error1_reported = true;
		}
	}

	before = OS::get_singleton()->get_ticks_msec();
	bool error2_reported = false;

	// Wait until all threads have done all their tasks
	bool any_working_thread = true;
	while (any_working_thread) {
		any_working_thread = false;
		for (size_t i = 0; i < _thread_count; ++i) {
			const ThreadData &t = _threads[i];
			if (t.waiting == false) {
				any_working_thread = true;
				break;
			}
		}

		OS::get_singleton()->delay_usec(1000);

		if (!error2_reported && OS::get_singleton()->get_ticks_msec() - before > suspicious_delay) {
			ERR_PRINT("Waiting for all tasks to be completed is taking too long");
			error2_reported = true;
		}
	}
}

VoxelThreadPool::State VoxelThreadPool::get_thread_debug_state(uint32_t i) const {
	return _threads[i].debug_state;
}
