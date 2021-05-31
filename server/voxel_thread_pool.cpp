#include "voxel_thread_pool.h"
#include "../util/profiling.h"

#include <core/os/os.h>
#include <core/os/semaphore.h>
#include <core/os/thread.h>

// template <typename T>
// static bool contains(const std::vector<T> vec, T v) {
// 	for (size_t i = 0; i < vec.size(); ++i) {
// 		if (vec[i] == v) {
// 			return true;
// 		}
// 	}
// 	return false;
// }

VoxelThreadPool::VoxelThreadPool() {
}

VoxelThreadPool::~VoxelThreadPool() {
	destroy_all_threads();

	if (_completed_tasks.size() != 0) {
		// We don't have ownership over tasks, so it's an error to destroy the pool without handling them
		ERR_PRINT("There are unhandled completed tasks remaining!");
	}
}

void VoxelThreadPool::create_thread(ThreadData &d, uint32_t i) {
	d.pool = this;
	d.stop = false;
	d.waiting = false;
	d.index = i;
	if (!_name.empty()) {
		d.name = String("{0} {1}").format(varray(_name, i));
	}
	d.thread.start(thread_func_static, &d);
}

void VoxelThreadPool::destroy_all_threads() {
	// We have only one semaphore to signal threads to resume, and one `post()` lets only one pass.
	// We cannot tell one single thread to stop, because when we post and other threads are waiting, we can't guarantee
	// the one to pass will be the one we want.
	// So we can only choose to stop ALL threads, and then start them again if we want to adjust their count.
	// Also, it shouldn't drop tasks. Any tasks the thread was working on should still complete normally.
	for (size_t i = 0; i < _thread_count; ++i) {
		ThreadData &d = _threads[i];
		d.stop = true;
	}
	for (size_t i = 0; i < _thread_count; ++i) {
		_tasks_semaphore.post();
	}
	for (size_t i = 0; i < _thread_count; ++i) {
		ThreadData &d = _threads[i];
		d.wait_to_finish_and_reset();
	}
}

void VoxelThreadPool::set_name(String name) {
	_name = name;
}

void VoxelThreadPool::set_thread_count(uint32_t count) {
	if (count > MAX_THREADS) {
		count = MAX_THREADS;
	}
	destroy_all_threads();
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
		++_debug_received_tasks;
	}
	// TODO Do I need to post a certain amount of times?
	_tasks_semaphore.post();
}

void VoxelThreadPool::enqueue(Span<IVoxelTask *> tasks) {
	{
		MutexLock lock(_tasks_mutex);
		for (size_t i = 0; i < tasks.size(); ++i) {
			TaskItem t;
			t.task = tasks[i];
			CRASH_COND(t.task == nullptr);
			_tasks.push_back(t);
			++_debug_received_tasks;
		}
	}
	// TODO Do I need to post a certain amount of times?
	for (size_t i = 0; i < tasks.size(); ++i) {
		_tasks_semaphore.post();
	}
}

void VoxelThreadPool::thread_func_static(void *p_data) {
	ThreadData &data = *static_cast<ThreadData *>(p_data);
	VoxelThreadPool &pool = *data.pool;

	if (!data.name.empty()) {
		Thread::set_name(data.name);

#ifdef VOXEL_PROFILER_ENABLED
		CharString thread_name = data.name.utf8();
		VOXEL_PROFILE_SET_THREAD_NAME(thread_name.get_data());
#endif
	}

	pool.thread_func(data);
}

void VoxelThreadPool::thread_func(ThreadData &data) {
	data.debug_state = STATE_RUNNING;

	std::vector<TaskItem> tasks;
	std::vector<IVoxelTask *> cancelled_tasks;

	while (!data.stop) {
		{
			VOXEL_PROFILE_SCOPE();

			data.debug_state = STATE_PICKING;
			const uint32_t now = OS::get_singleton()->get_ticks_msec();

			MutexLock lock(_tasks_mutex);

			// Pick best tasks
			for (uint32_t bi = 0; bi < _batch_count && _tasks.size() != 0; ++bi) {
				size_t best_index = 0; // Take first by default, this is a valid index
				int best_priority = 999999;

				// TODO This takes a lot of time when there are many queued tasks. Use a better container?
				for (size_t i = 0; i < _tasks.size(); ++i) {
					TaskItem &item = _tasks[i];
					CRASH_COND(item.task == nullptr);

					if (now - item.last_priority_update_time > _priority_update_period) {
						// Calling `get_priority()` first since it can update cancellation
						// (not clear API tho, might review that in the future)
						item.cached_priority = item.task->get_priority();

						if (item.task->is_cancelled()) {
							cancelled_tasks.push_back(item.task);
							_tasks[i] = _tasks.back();
							_tasks.pop_back();
							--i;
							continue;
						}

						item.last_priority_update_time = now;
					}

					if (item.cached_priority < best_priority) {
						best_priority = item.cached_priority;
						best_index = i;
					}
				}

				// If not all tasks were cancelled
				if (_tasks.size() != 0) {
					tasks.push_back(_tasks[best_index]);
					CRASH_COND(best_index >= _tasks.size());
					_tasks[best_index] = _tasks.back();
					_tasks.pop_back();
				}
			}
		}

		if (cancelled_tasks.size() > 0) {
			MutexLock lock(_completed_tasks_mutex);
			for (size_t i = 0; i < cancelled_tasks.size(); ++i) {
				_completed_tasks.push_back(cancelled_tasks[i]);
				++_debug_completed_tasks;
			}
		}
		cancelled_tasks.clear();

		//print_line(String("Processing {0} tasks").format(varray(tasks.size())));

		if (tasks.empty()) {
			data.debug_state = STATE_WAITING;

			// Wait for more tasks
			data.waiting = true;
			_tasks_semaphore.wait();
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
					++_debug_completed_tasks;
				}
			}

			tasks.clear();
		}
	}

	data.debug_state = STATE_STOPPED;
}

void VoxelThreadPool::wait_for_all_tasks() {
	const uint32_t suspicious_delay_msec = 10000;

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

		OS::get_singleton()->delay_usec(2000);

		if (!error1_reported && OS::get_singleton()->get_ticks_msec() - before > suspicious_delay_msec) {
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

		OS::get_singleton()->delay_usec(2000);

		if (!error2_reported && OS::get_singleton()->get_ticks_msec() - before > suspicious_delay_msec) {
			ERR_PRINT("Waiting for all tasks to be completed is taking too long");
			error2_reported = true;
		}
	}
}

// Debug information can be wrong, on some rare occasions.
// The variables should be safely updated, but computing or reading from them is not thread safe.
// Thought it wasnt worth locking for debugging.

VoxelThreadPool::State VoxelThreadPool::get_thread_debug_state(uint32_t i) const {
	return _threads[i].debug_state;
}

unsigned int VoxelThreadPool::get_debug_remaining_tasks() const {
	return _debug_received_tasks - _debug_completed_tasks;
}
