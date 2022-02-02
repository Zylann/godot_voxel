#ifndef THREADED_TASK_H
#define THREADED_TASK_H

#include <cstdint>

namespace zylann {

struct ThreadedTaskContext {
	uint8_t thread_index;
};

class IThreadedTask {
public:
	virtual ~IThreadedTask() {}

	// Called from within the thread pool
	virtual void run(ThreadedTaskContext ctx) = 0;

	// Convenience method which can be called by the scheduler of the task (usually on the main thread)
	// in order to apply results. It is not called from the thread pool.
	virtual void apply_result() = 0;

	// Lower values means higher priority.
	// Can change between two calls. The thread pool will poll this value regularly over some time interval.
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
