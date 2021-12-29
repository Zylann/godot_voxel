#ifndef ZYLANN_ASYNC_DEPENDENCY_TRACKER_H
#define ZYLANN_ASYNC_DEPENDENCY_TRACKER_H

#include "../util/span.h"
#include <atomic>
#include <vector>

namespace zylann {

class IThreadedTask;

// Tracks the status of one or more tasks.
class AsyncDependencyTracker {
public:
	// Creates a tracker which will track `initial_count` tasks.
	// The tracker may be passed by shared pointer to each of these tasks so they can notify completion.
	AsyncDependencyTracker(int initial_count);

	// Alternate constructor where a collection of tasks will be scheduled on completion.
	// All the next tasks will be run in parallel.
	// If a dependency is aborted, these tasks will be destroyed instead.
	AsyncDependencyTracker(int initial_count, Span<IThreadedTask *> next_tasks);

	~AsyncDependencyTracker();

	// Call this when one of the tracked dependencies is complete
	void post_complete();

	// Call this when one of the tracked dependencies is aborted
	void abort() {
		_aborted = true;
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
	std::vector<IThreadedTask *> _next_tasks;
};

} // namespace zylann

#endif // ZYLANN_ASYNC_DEPENDENCY_TRACKER_H
