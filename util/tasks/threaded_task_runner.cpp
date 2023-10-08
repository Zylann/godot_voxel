#include "threaded_task_runner.h"
#include "../container_funcs.h"
#include "../dstack.h"
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

	// We don't have ownership over tasks, so it's an error to destroy the pool without handling them
	if (_staged_tasks.size() != 0) {
		ZN_PRINT_ERROR("There are staged tasks remaining!");
	}
	if (_tasks.size() != 0) {
		ZN_PRINT_ERROR("There are tasks remaining!");
	}
	if (_spinning_tasks.size() != 0) {
		ZN_PRINT_ERROR("There are spinning tasks remaining!");
	}
	if (_completed_tasks.size() != 0) {
		ZN_PRINT_ERROR("There are completed tasks remaining!");
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

#ifdef ZN_THREADED_TASK_RUNNER_CHECK_DUPLICATE_TASKS

void ThreadedTaskRunner::debug_add_owned_task(IThreadedTask *task) {
	std::string s;
	dstack::Info info;
	info.to_string(s);
	println(format("Own {} t {}", uint64_t(task), Thread::get_caller_id()));
	MutexLock mlock(_debug_owned_tasks_mutex);
	auto p = _debug_owned_tasks.insert({ task, s });
	if (p.second == false) {
		flush_log_file();
		ZN_CRASH();
	}
}

void ThreadedTaskRunner::debug_remove_owned_task(IThreadedTask *task) {
	{
		MutexLock mlock(_debug_owned_tasks_mutex);
		ZN_ASSERT(_debug_owned_tasks.erase(task) == 1);
	}
	println(format("Unowned {}", uint64_t(task)));
}

#endif // ZN_THREADED_TASK_RUNNER_CHECK_DUPLICATE_TASKS

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

void ThreadedTaskRunner::set_priority_update_period(uint32_t milliseconds) {
	_priority_update_period_ms = milliseconds;
}

void ThreadedTaskRunner::enqueue(IThreadedTask *task, bool serial) {
	ZN_PROFILE_SCOPE();
	ZN_ASSERT(task != nullptr);
	TaskItem t;
	t.task = task;
	t.is_serial = serial;
	{
		MutexLock lock(_staged_tasks_mutex);
		_staged_tasks.push_back(t);
		++_debug_received_tasks;

#ifdef ZN_THREADED_TASK_RUNNER_CHECK_DUPLICATE_TASKS
		debug_add_owned_task(task);
#endif
	}
	// TODO Do I need to post a certain amount of times?
	// I feel like this causes the semaphore to be passed too many times when tasks become empty
	_tasks_semaphore.post();
}

void ThreadedTaskRunner::enqueue(Span<IThreadedTask *> new_tasks, bool serial) {
#ifdef DEBUG_ENABLED
	for (size_t i = 0; i < new_tasks.size(); ++i) {
		ZN_ASSERT(new_tasks[i] != nullptr);
	}
#endif
	{
		MutexLock lock(_staged_tasks_mutex);
		const size_t dst_begin = _staged_tasks.size();
		_staged_tasks.resize(_staged_tasks.size() + new_tasks.size());
		for (size_t i = 0; i < new_tasks.size(); ++i) {
			IThreadedTask *new_task = new_tasks[i];
			TaskItem t;
			t.task = new_task;
			t.is_serial = serial;
			_staged_tasks[dst_begin + i] = t;

#ifdef ZN_THREADED_TASK_RUNNER_CHECK_DUPLICATE_TASKS
			debug_add_owned_task(new_task);
#endif
		}
		_debug_received_tasks += new_tasks.size();
	}
	// TODO Do I need to post a certain amount of times?
	// Should it be the number of threads instead of number of tasks?
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
	std::vector<TaskItem> postponed_tasks;
	std::vector<IThreadedTask *> cancelled_tasks;

	while (!data.stop) {
		bool is_running_serial_task = false;
		bool task_queue_was_empty = false;
		{
			ZN_PROFILE_SCOPE_NAMED("Task pickup");

			data.debug_state = STATE_PICKING;

			ZN_ASSERT(tasks.size() == 0);

			// Pick a postponed task if any.
			// We will still run a task from the main prioritized queue as well so postponed tasks will not
			// monopolize execution.
			//
			// TODO What if postponed tasks remain while one big task is locking what they need to access?
			// Those postponed tasks will sort of spinlock with no sleeping. Is that a bad thing?
			{
				MutexLock lock2(_spinning_tasks_mutex);
				if (_spinning_tasks.size() > 0) {
					tasks.push_back(_spinning_tasks.front());
					_spinning_tasks.pop();
				}
			}

			{
				// TODO When tasks are very short and there are a lot of tasks, one thread can monopolize this mutex.
				//
				MutexLock lock(_tasks_mutex);

				// Move tasks from the staging queue.
				// Lock with minimal risk of blocking the main thread, it should be very short.
				if (_staged_tasks_mutex.try_lock()) {
					append_array(_tasks, _staged_tasks);
					_staged_tasks.clear();
					_staged_tasks_mutex.unlock();
				}

				// Pick best tasks from the prioritized queue
				if (_tasks.size() != 0) {
					// Sort periodically.
					// The point to keep sorting after tasks have been inserted is in case there are lots of pending
					// tasks, which can take more than a few seconds to be processed. A player can move fast and the
					// priority location can change. Some tasks can even become irrelevant before they are run,so we
					// may remove them from the list so they don't slow down the process.
					const uint64_t now = Time::get_singleton()->get_ticks_msec();
					if (now - _last_priority_update_time_ms > _priority_update_period_ms) {
						ZN_PROFILE_SCOPE_NAMED("Sorting");

						{
							ZN_PROFILE_SCOPE_NAMED("Update priorities");
							for (unsigned int i = 0; i < _tasks.size();) {
								TaskItem &item = _tasks[i];
								item.cached_priority = item.task->get_priority();

								if (item.task->is_cancelled()) {
									cancelled_tasks.push_back(item.task);
									_tasks[i] = _tasks.back();
									_tasks.pop_back();
									continue;
								}

								++i;
							}
						}

						struct TaskComparator {
							inline bool operator()(const TaskItem &a, const TaskItem &b) const {
								// Tasks with highest priority come last (easier pop back)
								return a.cached_priority < b.cached_priority;
							}
						};
						SortArray<TaskItem, TaskComparator> sorter;
						sorter.sort(_tasks.data(), _tasks.size());

						_last_priority_update_time_ms = Time::get_singleton()->get_ticks_msec();
					}

					// Pick task with highest priority if possible
					// for (int i = int(_tasks.size()) - 1; i >= 0; --i) {
					for (unsigned int i = _tasks.size(); i-- > 0;) {
						const TaskItem item = _tasks[i];
						// Serial tasks are a bit annoying in that regard...
						// We could make the save/load tasks accept more than one work, which is the best way to do
						// serial work, but in some cases it's harder to know in advance...
						if (item.is_serial && _is_serial_task_running) {
							// Try previous task
							continue;
						}

						tasks.push_back(item);
						// We don't just pop the last item because of serial task handling. But ordered removal should
						// be fast enough since serial tasks aren't common.
						_tasks.erase(_tasks.begin() + i);
						break;
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
			const size_t count = cancelled_tasks.size();
			append_array(_completed_tasks, cancelled_tasks);
			_debug_completed_tasks += count;
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
					ThreadedTaskContext ctx{ uint8_t(data.index),
						// By default, if the task does not set this status, it will be considered complete after run
						ThreadedTaskContext::STATUS_COMPLETE, item.cached_priority };
					data.debug_running_task_name = item.task->get_debug_name();
					item.task->run(ctx);
#ifdef ZN_THREADED_TASK_RUNNER_CHECK_DUPLICATE_TASKS
					if (ctx.status == ThreadedTaskContext::STATUS_TAKEN_OUT) {
						debug_remove_owned_task(item.task);
					}
#endif
					item.status = ctx.status;
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
					const TaskItem &item = tasks[i];
					switch (item.status) {
						case ThreadedTaskContext::STATUS_COMPLETE:
							_completed_tasks.push_back(item.task);
							++_debug_completed_tasks;
							break;

						case ThreadedTaskContext::STATUS_POSTPONED:
							postponed_tasks.push_back(item);
							break;

						case ThreadedTaskContext::STATUS_TAKEN_OUT:
							// Drop task pointer, its ownership may have been passed to another task
							++_debug_taken_out_tasks;
							break;

						default:
							ZN_PRINT_ERROR("Unknown task status");
							break;
					}
				}
			}

			tasks.clear();

			{
				MutexLock lock(_spinning_tasks_mutex);
				for (const TaskItem &item : postponed_tasks) {
					_spinning_tasks.push(item);
				}
			}

			postponed_tasks.clear();
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
		// TODO this is not really precise, because running tasks can schedule more tasks. Not sure if we need it?
		// Waiting for all threads to be in waiting state is a more definitive solution.
		bool any_staged_tasks = false;
		{
			MutexLock lock3(_staged_tasks_mutex);
			any_staged_tasks = _staged_tasks.size() > 0;
		}
		if (!any_staged_tasks) {
			MutexLock lock(_tasks_mutex);
			if (_tasks.size() == 0) {
				MutexLock lock2(_spinning_tasks_mutex);
				if (_spinning_tasks.size() == 0) {
					break;
				}
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
	return _debug_received_tasks - _debug_completed_tasks - _debug_taken_out_tasks;
}

std::vector<IThreadedTask *> &ThreadedTaskRunner::get_completed_tasks_temp_tls() {
	static thread_local std::vector<IThreadedTask *> tls_temp;
	return tls_temp;
}

} // namespace zylann
