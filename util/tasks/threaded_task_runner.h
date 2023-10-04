#ifndef ZYLANN_THREADED_TASK_RUNNER_H
#define ZYLANN_THREADED_TASK_RUNNER_H

#include "../container_funcs.h"
#include "../fixed_array.h"
#include "../profiling.h"
#include "../span.h"
#include "../thread/mutex.h"
#include "../thread/semaphore.h"
#include "../thread/thread.h"
#include "threaded_task.h"

// For debugging
// #define ZN_THREADED_TASK_RUNNER_CHECK_DUPLICATE_TASKS

#ifdef ZN_THREADED_TASK_RUNNER_CHECK_DUPLICATE_TASKS
#include <unordered_map>
#endif

#include <atomic>
#include <queue>
#include <string>

namespace zylann {

// Generic thread pool that performs batches of tasks based on dynamic priority
class ThreadedTaskRunner {
public:
	static const uint32_t MAX_THREADS = 16;

	enum State { //
		STATE_RUNNING = 0,
		STATE_PICKING,
		STATE_WAITING,
		STATE_STOPPED
	};

	ThreadedTaskRunner();
	~ThreadedTaskRunner();

	// Set name prefix to recognize threads of this pool in debug tools.
	// Must be called before configuring thread count.
	void set_name(const char *name);

	// TODO Add ability to change it while running without skipping tasks
	// Can't be changed after tasks have been queued
	void set_thread_count(uint32_t count);
	uint32_t get_thread_count() const {
		return _thread_count;
	}

	// TODO Add ability to change it while running
	// Task priorities can change over time, but computing them too often with many tasks can be expensive,
	// so they are cached. This sets how often task priorities will be polled.
	// Can't be changed after tasks have been queued.
	void set_priority_update_period(uint32_t milliseconds);

	// TODO Expect tasks to be unique ptrs?

	// Schedules a task.
	// Ownership is NOT passed to the pool, so make sure you get them back when completed if you want to delete them.
	// All tasks scheduled with `serial=true` will run one after the other, using one thread at a time.
	// Tasks scheduled with `serial=false` can run in parallel using multiple threads.
	// Serial execution is useful when such tasks cannot run in parallel due to locking a shared resource. This avoids
	// clogging up all threads with waiting tasks.
	void enqueue(IThreadedTask *task, bool serial);
	// Schedules multiple tasks at once. Involves less internal locking.
	void enqueue(Span<IThreadedTask *> new_tasks, bool serial);

	template <typename F>
	void dequeue_completed_tasks(F f) {
		ZN_PROFILE_SCOPE();
		std::vector<IThreadedTask *> &temp = get_completed_tasks_temp_tls();
		ZN_ASSERT(temp.size() == 0);
		{
			MutexLock lock(_completed_tasks_mutex);
			append_array(temp, _completed_tasks);
			_completed_tasks.clear();
			// std::move doesn't guarantee preservation of vector capacity
			// temp = std::move(_completed_tasks);
		}
		for (IThreadedTask *task : temp) {
#ifdef ZN_THREADED_TASK_RUNNER_CHECK_DUPLICATE_TASKS
			debug_remove_owned_task(task);
#endif
			f(task);
		}
		temp.clear();
	}

	// Blocks and wait for all tasks to finish (assuming no more are getting added!)
	void wait_for_all_tasks();

	State get_thread_debug_state(uint32_t i) const;
	const char *get_thread_debug_task_name(unsigned int thread_index) const;
	unsigned int get_debug_remaining_tasks() const;

private:
	static std::vector<IThreadedTask *> &get_completed_tasks_temp_tls();

	struct TaskItem {
		IThreadedTask *task = nullptr;
		TaskPriority cached_priority;
		bool is_serial = false;
		uint8_t status = ThreadedTaskContext::STATUS_COMPLETE;
	};

	struct ThreadData {
		Thread thread;
		ThreadedTaskRunner *pool = nullptr;
		uint32_t index = 0;
		bool stop = false;
		bool waiting = false;
		State debug_state = STATE_STOPPED;
		std::string name;
		std::atomic<const char *> debug_running_task_name = { nullptr };

		void wait_to_finish_and_reset() {
			thread.wait_to_finish();
			pool = nullptr;
			index = 0;
			stop = false;
			waiting = false;
			debug_state = STATE_STOPPED;
			name.clear();
		}
	};

	static void thread_func_static(void *p_data);
	void thread_func(ThreadData &data);

	void create_thread(ThreadData &d, uint32_t i);
	void destroy_all_threads();

#ifdef ZN_THREADED_TASK_RUNNER_CHECK_DUPLICATE_TASKS
	void debug_add_owned_task(IThreadedTask *task);
	void debug_remove_owned_task(IThreadedTask *task);
#endif

	FixedArray<ThreadData, MAX_THREADS> _threads;
	uint32_t _thread_count = 0;

	// Scheduled tasks are put here first. They will be moved to the main waiting queue by the next available thread.
	// This is because the main waiting queue can be locked for longer due to dynamic priority sorting.
	std::vector<TaskItem> _staged_tasks;
	Mutex _staged_tasks_mutex;

	// Main waiting list. Tasks are picked from it by priority. Priority can also change while tasks are in this list,
	// so we can't use a simple queue or sort at insertion. Every available thread has to find it and potentially update
	// it every once in a while.
	std::vector<TaskItem> _tasks;
	Mutex _tasks_mutex;
	Semaphore _tasks_semaphore;

	// Ongoing tasks that may take more than one iteration
	std::queue<TaskItem> _spinning_tasks;
	Mutex _spinning_tasks_mutex;

	std::vector<IThreadedTask *> _completed_tasks;
	Mutex _completed_tasks_mutex;

	uint32_t _priority_update_period_ms = 32;
	uint64_t _last_priority_update_time_ms = 0;

	// This boolean is also guarded with `_tasks_mutex`.
	// Tasks marked as "serial" must be executed by only one thread at a time.
	bool _is_serial_task_running = false;

	std::string _name;

	unsigned int _debug_received_tasks = 0;
	unsigned int _debug_completed_tasks = 0;
	unsigned int _debug_taken_out_tasks = 0;

#ifdef ZN_THREADED_TASK_RUNNER_CHECK_DUPLICATE_TASKS
	std::unordered_map<IThreadedTask *, std::string> _debug_owned_tasks;
	Mutex _debug_owned_tasks_mutex;
#endif
};

} // namespace zylann

#endif // ZYLANN_THREADED_TASK_RUNNER_H
