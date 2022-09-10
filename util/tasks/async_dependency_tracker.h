#ifndef ZYLANN_ASYNC_DEPENDENCY_TRACKER_H
#define ZYLANN_ASYNC_DEPENDENCY_TRACKER_H

#include "../span.h"
#include <atomic>
#include <vector>

namespace zylann {

class IThreadedTask;

// Tracks the status of one or more tasks.
// This should be referenced by tasks using a shared pointer.
class AsyncDependencyTracker {
public:
	// Creates a tracker with no known tasks to track yet. You must use `set_count` before scheduling tasks.
	AsyncDependencyTracker();

	// Creates a tracker which will track `initial_count` tasks.
	AsyncDependencyTracker(int initial_count);

	typedef void (*ScheduleNextTasksCallback)(Span<IThreadedTask *> tasks);

	// Alternate constructor where a collection of tasks will be scheduled on completion.
	// All the next tasks will be run in parallel.
	// If a dependency is aborted, these tasks will be destroyed instead.
	AsyncDependencyTracker(int initial_count, Span<IThreadedTask *> next_tasks, ScheduleNextTasksCallback scheduler_cb);

	~AsyncDependencyTracker();

	// Sets dependency count. This may only be used if you don't know easily the amount of tasks to create up-front, but
	// has to be called BEFORE those tasks are scheduled.
	void set_count(int count);

	// Call this when one of the tracked dependencies is complete
	void post_complete();

	// Call this when one of the tracked dependencies is aborted
	void abort() {
		_aborted = true;
		_tasks_have_started = true;
	}

	// Returns `true` if any of the tracked tasks was aborted.
	// It usually means tasks depending on this tracker may be aborted as well.
	bool is_aborted() const {
		return _aborted;
	}

	// Returns `true` when all the tracked tasks have completed
	bool is_complete() const {
		return _count == 0;
	}

	int get_remaining_count() const {
		return _count;
	}

	bool has_next_tasks() const {
		return _next_tasks.size() > 0;
	}

private:
	std::atomic_int _count;
	std::atomic_bool _aborted;
	std::atomic_bool _tasks_have_started;
	bool _count_was_set = false;
	std::vector<IThreadedTask *> _next_tasks;
	ScheduleNextTasksCallback _next_tasks_schedule_callback = nullptr;
};

} // namespace zylann

#endif // ZYLANN_ASYNC_DEPENDENCY_TRACKER_H
