#ifndef THREADED_TASK_H
#define THREADED_TASK_H

#include <cstdint>

namespace zylann {

struct ThreadedTaskContext {
	uint8_t thread_index;
};

// Interface for a task that will run in `ThreadedTaskRunner`.
// The task will run in another thread.
class IThreadedTask {
public:
	virtual ~IThreadedTask() {}

	// Called from within the thread pool
	virtual void run(ThreadedTaskContext ctx) = 0;

	// Convenience method which can be called by the scheduler of the task (usually on the main thread)
	// in order to apply results. It is not called from the thread pool.
	virtual void apply_result(){};

	// Hints how soon this task will be executed after being scheduled. This is relevant when there are a lot of tasks.
	// Lower values means higher priority.
	// Can change between two calls. The thread pool will poll this value regularly over some time interval.
	// TODO Should we disallow negative values? I can't think of a use for it.
	virtual int get_priority() {
		return 0;
	}

	// May return `true` in order for the thread pool to skip the task
	virtual bool is_cancelled() {
		return false;
	}
};

} // namespace zylann

#endif // THREADED_TASK_H
