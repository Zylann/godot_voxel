#ifndef THREADED_TASK_H
#define THREADED_TASK_H

#include "task_priority.h"
#include <cstdint>

namespace zylann {

struct ThreadedTaskContext {
	enum Status {
		// The task is complete and will be put in the list of completed tasks by the TaskRunner. It will be deleted
		// later. This is the default status.
		STATUS_COMPLETE = 0,
		// The task is not complete and will be re-run later by the TaskRunner
		STATUS_POSTPONED = 1,
		// The task is not complete and will be re-scheduled by another custom task.
		// The TaskRunner will simply drop its pointer and won't put it in the list of completed tasks.
		// Initially added so we can schedule task B from task A, and have B re-schedule A to use results computed in A
		STATUS_TAKEN_OUT = 2
	};

	// Index of the thread within the runner's pool. Can be used to index arrays as an alternative to thread_local
	// storage.
	const uint8_t thread_index;
	// May be set by the task to signal its status after run
	Status status;
};

// Interface for a task that will run in `ThreadedTaskRunner`.
// The task will run in another thread.
class IThreadedTask {
public:
	virtual ~IThreadedTask() {}

	// Called from within the thread pool
	virtual void run(ThreadedTaskContext &ctx) = 0;

	// Convenience method which can be called by the scheduler of the task (usually on the main thread)
	// in order to apply results. It is not called from the thread pool.
	virtual void apply_result(){};

	// Hints how soon this task will be executed after being scheduled. This is relevant when there are a lot of tasks.
	// Lower values means higher priority.
	// Can change between two calls. The thread pool will poll this value regularly over some time interval.
	virtual TaskPriority get_priority() {
		// Defaulting to maximum priority as it's the most common expectation.
		return TaskPriority::max();
	}

	// May return `true` in order for the thread pool to skip the task
	virtual bool is_cancelled() {
		return false;
	}

	// Gets the name of the task for debug purposes. The returned name's lifetime must span the execution of the engine
	// (usually a string literal).
	virtual const char *get_debug_name() const {
		return "<unnamed>";
	}
};

} // namespace zylann

#endif // THREADED_TASK_H
