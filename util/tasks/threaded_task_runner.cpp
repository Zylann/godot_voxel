#include "threaded_task_runner.h"
#include "../godot/classes/time.h"
#include "../profiling.h"
#include "../string_funcs.h"

namespace zylann {

// template <typename T>
// static bool contains(const std::vector<T> vec, T v) {
// 	for (size_t i = 0; i < vec.size(); ++i) {
// 		if (vec[i] == v) {
// 			return true;
// 		}
// 	}
// 	return false;
// }

ThreadedTaskRunner::ThreadedTaskRunner() {}

ThreadedTaskRunner::~ThreadedTaskRunner() {
	destroy_all_threads();

	if (_completed_tasks.size() != 0) {
		// We don't have ownership over tasks, so it's an error to destroy the pool without handling them
		ZN_PRINT_ERROR("There are unhandled completed tasks remaining!");
	}
}

void ThreadedTaskRunner::create_thread(ThreadData &d, uint32_t i) {
	d.pool = this;
	d.stop = false;
	d.waiting = false;
	d.index = i;
	if (!_name.empty()) {
		d.name = format("{} {}", _name, i);
	}
	d.thread.start(thread_func_static, &d);
}

void ThreadedTaskRunner::destroy_all_threads() {
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

void ThreadedTaskRunner::set_name(const char *name) {
	_name = name;
}

void ThreadedTaskRunner::set_thread_count(uint32_t count) {
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

void ThreadedTaskRunner::set_batch_count(uint32_t count) {
	_batch_count = count;
}

void ThreadedTaskRunner::set_priority_update_period(uint32_t milliseconds) {
	_priority_update_period_ms = milliseconds;
}

void ThreadedTaskRunner::enqueue(IThreadedTask *task, bool serial) {
	ZN_ASSERT(task != nullptr);
	TaskItem t;
	t.task = task;
	t.is_serial = serial;
	{
		MutexLock lock(_tasks_mutex);
		_tasks.push_back(t);
		++_debug_received_tasks;
	}
	// TODO Do I need to post a certain amount of times?
	_tasks_semaphore.post();
}

void ThreadedTaskRunner::enqueue(Span<IThreadedTask *> new_tasks, bool serial) {
#ifdef DEBUG_ENABLED
	for (size_t i = 0; i < new_tasks.size(); ++i) {
		ZN_ASSERT(new_tasks[i] != nullptr);
	}
#endif
	{
		MutexLock lock(_tasks_mutex);
		const size_t dst_begin = _tasks.size();
		_tasks.resize(_tasks.size() + new_tasks.size());
		for (size_t i = 0; i < new_tasks.size(); ++i) {
			TaskItem t;
			t.task = new_tasks[i];
			t.is_serial = serial;
			_tasks[dst_begin + i] = t;
		}
		_debug_received_tasks += new_tasks.size();
	}
	// TODO Do I need to post a certain amount of times?
	for (size_t i = 0; i < new_tasks.size(); ++i) {
		_tasks_semaphore.post();
	}
}

void ThreadedTaskRunner::thread_func_static(void *p_data) {
	ThreadData &data = *static_cast<ThreadData *>(p_data);
	ThreadedTaskRunner &pool = *data.pool;

	if (!data.name.empty()) {
		Thread::set_name(data.name.c_str());

#ifdef ZN_PROFILER_ENABLED
		ZN_PROFILE_SET_THREAD_NAME(data.name.c_str());
#endif
	}

	pool.thread_func(data);
}

void ThreadedTaskRunner::thread_func(ThreadData &data) {
	data.debug_state = STATE_RUNNING;

	std::vector<TaskItem> tasks;
	std::vector<IThreadedTask *> cancelled_tasks;

	while (!data.stop) {
		bool is_running_serial_task = false;
		bool task_queue_was_empty = false;
		{
			ZN_PROFILE_SCOPE_NAMED("Task pickup");

			data.debug_state = STATE_PICKING;
			const uint64_t now = Time::get_singleton()->get_ticks_msec();

			{
				MutexLock lock(_tasks_mutex);

				// Pick best tasks
				for (uint32_t bi = 0; bi < _batch_count && _tasks.size() != 0; ++bi) {
					size_t best_index = 0; // Take first by default, this is a valid index
					TaskPriority highest_priority = TaskPriority::min();
					bool picked_task = false;

					// Find best task to pick
					// TODO Optimize: this takes a lot of time when there are many queued tasks. Use a better container?
					for (size_t i = 0; i < _tasks.size();) {
						TaskItem &item = _tasks[i];
						ZN_ASSERT(item.task != nullptr);

						// Process priority and cancellation
						if (now - item.last_priority_update_time_ms > _priority_update_period_ms) {
							// Calling `get_priority()` first since it can update cancellation
							// (not clear API tho, might review that in the future)
							item.cached_priority = item.task->get_priority();

							if (item.task->is_cancelled()) {
								cancelled_tasks.push_back(item.task);
								_tasks[i] = _tasks.back();
								_tasks.pop_back();
								continue;
							}

							item.last_priority_update_time_ms = now;
						}

						// Pick item if it has better priority.
						// If the item is serial, there must not be a serial task already running.
						if (item.cached_priority > highest_priority && !(item.is_serial && _is_serial_task_running)) {
							highest_priority = item.cached_priority;
							// This index should remain valid even if some tasks are removed because the "remove and
							// swap back" technique only affects items coming after
							best_index = i;
							picked_task = true;
						}

						++i;
					}

					if (picked_task) {
						tasks.push_back(_tasks[best_index]);
						ZN_ASSERT(best_index < _tasks.size());
						_tasks[best_index] = _tasks.back();
						_tasks.pop_back();
					}

				} // For each task to pick

				// If we picked up a serial task, we must set the shared boolean to `true`.
				// More than one serial task can be in the list of tasks the current thread picks up,
				// so we update the boolean after picking them all.
				// This must be the only place it can be set to `true`, and is guarded by mutex.
				if (_is_serial_task_running == false) { // Only an optimization, this doesnt actually do thread-safety
					for (unsigned int i = 0; i < tasks.size(); ++i) {
						if (tasks[i].is_serial) {
							// Write to member var so all threads can check this
							_is_serial_task_running = true;
							// Write to thread-local variable so we know it is the current thread
							is_running_serial_task = true;
							break;
						}
					}
				}

				task_queue_was_empty = _tasks.size() == 0;

			} // Tasks queue mutex lock
		}

		if (cancelled_tasks.size() > 0) {
			MutexLock lock(_completed_tasks_mutex);
			for (size_t i = 0; i < cancelled_tasks.size(); ++i) {
				_completed_tasks.push_back(cancelled_tasks[i]);
				++_debug_completed_tasks;
			}
			cancelled_tasks.clear();
		}

		// print_line(String("Processing {0} tasks").format(varray(tasks.size())));

		if (tasks.empty()) {
			if (task_queue_was_empty) {
				// The task queue is empty, will wait until more tasks are posted.
				// If a task is posted between the moment we last checked the queue and now,
				// the semaphore will have one count to decrement and we'll not stop here.

				data.debug_state = STATE_WAITING;

				// Wait for more tasks
				data.waiting = true;
				_tasks_semaphore.wait();
				data.waiting = false;

			} else {
				// The current thread was not allowed to pick tasks currently in the queue (they could be serial and one
				// is already running). So we'll wait for a very short time before retrying.
				// (alternative would be to post the semaphore after each serial task?)
				Thread::sleep_usec(1000);
			}

		} else {
			data.debug_state = STATE_RUNNING;

			// Run each task
			for (size_t i = 0; i < tasks.size(); ++i) {
				TaskItem &item = tasks[i];
				if (!item.task->is_cancelled()) {
					ThreadedTaskContext ctx;
					ctx.thread_index = data.index;
					data.debug_running_task_name = item.task->get_debug_name();
					item.task->run(ctx);
					data.debug_running_task_name = nullptr;
				}
			}

			// If the current thread just ran serial tasks
			if (is_running_serial_task) {
				ZN_ASSERT(_is_serial_task_running);
				// Reset back the boolean so any thread can pick serial tasks now.
				// This is the only place we set it to `false`, and can only be `true` already when that happens,
				// so locking the mutex should not be necessary.
				_is_serial_task_running = false;
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

void ThreadedTaskRunner::wait_for_all_tasks() {
	const uint32_t suspicious_delay_msec = 10'000;

	uint32_t before = Time::get_singleton()->get_ticks_msec();
	bool error1_reported = false;

	// Wait until all tasks have been taken
	while (true) {
		{
			MutexLock lock(_tasks_mutex);
			if (_tasks.size() == 0) {
				break;
			}
		}

		Thread::sleep_usec(2000);

		if (!error1_reported && Time::get_singleton()->get_ticks_msec() - before > suspicious_delay_msec) {
			ZN_PRINT_WARNING("Waiting for all tasks to be picked is taking a long time");
			error1_reported = true;
		}
	}

	before = Time::get_singleton()->get_ticks_msec();
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

		Thread::sleep_usec(2000);

		if (!error2_reported && Time::get_singleton()->get_ticks_msec() - before > suspicious_delay_msec) {
			ZN_PRINT_WARNING("Waiting for all tasks to be completed is taking a long time");
			error2_reported = true;
		}
	}
}

// Debug information can be wrong, on some rare occasions.
// The variables should be safely updated, but computing or reading from them is not thread safe.
// Thought it wasnt worth locking for debugging.

ThreadedTaskRunner::State ThreadedTaskRunner::get_thread_debug_state(uint32_t i) const {
	return _threads[i].debug_state;
}

const char *ThreadedTaskRunner::get_thread_debug_task_name(unsigned int thread_index) const {
	return _threads[thread_index].debug_running_task_name;
}

unsigned int ThreadedTaskRunner::get_debug_remaining_tasks() const {
	return _debug_received_tasks - _debug_completed_tasks;
}

} // namespace zylann
