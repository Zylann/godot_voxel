#ifndef ZYLANN_THREADED_TASK_RUNNER_H
#define ZYLANN_THREADED_TASK_RUNNER_H

#include "../fixed_array.h"
#include "../span.h"
#include "../thread/mutex.h"
#include "../thread/semaphore.h"
#include "../thread/thread.h"
#include "threaded_task.h"

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
	// Sets how many tasks each thread will attempt to dequeue on each iteration.
	// Can't be changed after tasks have been queued.
	void set_batch_count(uint32_t count);

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

	// TODO Optimization: lambda might not be the best API. memcpying to a vector would ensure we lock for a shorter
	// time.
	template <typename F>
	void dequeue_completed_tasks(F f) {
		MutexLock lock(_completed_tasks_mutex);
		for (size_t i = 0; i < _completed_tasks.size(); ++i) {
			IThreadedTask *task = _completed_tasks[i];
			f(task);
		}
		_completed_tasks.clear();
	}

	// Blocks and wait for all tasks to finish (assuming no more are getting added!)
	void wait_for_all_tasks();

	State get_thread_debug_state(uint32_t i) const;
	const char *get_thread_debug_task_name(unsigned int thread_index) const;
	unsigned int get_debug_remaining_tasks() const;

private:
	struct TaskItem {
		IThreadedTask *task = nullptr;
		TaskPriority cached_priority;
		bool is_serial = false;
		uint64_t last_priority_update_time_ms = 0;
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

	FixedArray<ThreadData, MAX_THREADS> _threads;
	uint32_t _thread_count = 0;

	// TODO Optimization: use a less naive design? Maybe moodycamel
	std::vector<TaskItem> _tasks;
	Mutex _tasks_mutex;
	Semaphore _tasks_semaphore;

	std::vector<IThreadedTask *> _completed_tasks;
	Mutex _completed_tasks_mutex;

	// TODO Remove batching, it's not really useful
	uint32_t _batch_count = 1;
	uint32_t _priority_update_period_ms = 32;

	// This boolean is also guarded with `_tasks_mutex`.
	// Tasks marked as "serial" must be executed by only one thread at a time.
	bool _is_serial_task_running = false;

	std::string _name;

	unsigned int _debug_received_tasks = 0;
	unsigned int _debug_completed_tasks = 0;
};

} // namespace zylann

#endif // ZYLANN_THREADED_TASK_RUNNER_H
