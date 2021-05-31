#ifndef VOXEL_THREAD_POOL_H
#define VOXEL_THREAD_POOL_H

#include "../storage/voxel_buffer.h"
#include "../util/fixed_array.h"
#include "../util/span.h"
#include <core/os/mutex.h>
#include <core/os/semaphore.h>
#include <core/os/thread.h>

#include <queue>

class Thread;

struct VoxelTaskContext {
	uint8_t thread_index;
};

class IVoxelTask {
public:
	virtual ~IVoxelTask() {}

	virtual void run(VoxelTaskContext ctx) = 0;

	// Lower values means higher priority
	virtual int get_priority() { return 0; }

	virtual bool is_cancelled() { return false; }
};

// Generic thread pool that performs batches of tasks based on priority
class VoxelThreadPool {
public:
	static const uint32_t MAX_THREADS = 8;

	enum State {
		STATE_RUNNING = 0,
		STATE_PICKING,
		STATE_WAITING,
		STATE_STOPPED
	};

	VoxelThreadPool();
	~VoxelThreadPool();

	// Set name prefix to recognize threads of this pool in debug tools.
	// Must be called before configuring thread count.
	void set_name(String name);

	// TODO Add ability to change it while running without skipping tasks
	// Can't be changed after tasks have been queued
	void set_thread_count(uint32_t count);
	uint32_t get_thread_count() const { return _thread_count; }

	// TODO Add ability to change it while running
	// Can't be changed after tasks have been queued
	void set_batch_count(uint32_t count);

	// TODO Add ability to change it while running
	// Can't be changed after tasks have been queued
	void set_priority_update_period(uint32_t milliseconds);

	// Schedules a task.
	// Ownership is NOT passed to the pool, so make sure you get them back when completed if you want to delete them.
	void enqueue(IVoxelTask *task);
	void enqueue(Span<IVoxelTask *> tasks);

	// TODO Lambda might not be the best API. memcpying to a vector would ensure we lock for a shorter time.
	template <typename F>
	void dequeue_completed_tasks(F f) {
		MutexLock lock(_completed_tasks_mutex);
		for (size_t i = 0; i < _completed_tasks.size(); ++i) {
			IVoxelTask *task = _completed_tasks[i];
			f(task);
		}
		_completed_tasks.clear();
	}

	// Blocks and wait for all tasks to finish (assuming no more are getting added!)
	void wait_for_all_tasks();

	State get_thread_debug_state(uint32_t i) const;
	unsigned int get_debug_remaining_tasks() const;

private:
	struct TaskItem {
		IVoxelTask *task = nullptr;
		int cached_priority = 99999;
		uint32_t last_priority_update_time = 0;
	};

	struct ThreadData {
		Thread thread;
		VoxelThreadPool *pool = nullptr;
		uint32_t index = 0;
		bool stop = false;
		bool waiting = false;
		State debug_state = STATE_STOPPED;
		String name;

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

	// TODO Optimize this with a less naive design? Maybe moodycamel
	std::vector<TaskItem> _tasks;
	Mutex _tasks_mutex;
	Semaphore _tasks_semaphore;

	std::vector<IVoxelTask *> _completed_tasks;
	Mutex _completed_tasks_mutex;

	uint32_t _batch_count = 1;
	uint32_t _priority_update_period = 32;

	String _name;

	unsigned int _debug_received_tasks = 0;
	unsigned int _debug_completed_tasks = 0;
};

#endif // VOXEL_THREAD_TASK_MANAGER_H
