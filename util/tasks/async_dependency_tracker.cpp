#include "async_dependency_tracker.h"
#include "../memory.h"
#include "threaded_task_runner.h"

namespace zylann {

AsyncDependencyTracker::AsyncDependencyTracker() :
		_count(0), _aborted(false), _tasks_have_started(false), _count_was_set(false) {}

AsyncDependencyTracker::AsyncDependencyTracker(int initial_count) :
		_count(initial_count), _aborted(false), _tasks_have_started(false), _count_was_set(true) {}

AsyncDependencyTracker::AsyncDependencyTracker(
		int initial_count, Span<IThreadedTask *> next_tasks, ScheduleNextTasksCallback scheduler_cb) :
		_count(initial_count),
		_aborted(false),
		_tasks_have_started(false),
		_count_was_set(true),
		_next_tasks_schedule_callback(scheduler_cb) {
	//
	ZN_ASSERT(scheduler_cb != nullptr);

	_next_tasks.resize(next_tasks.size());

	for (unsigned int i = 0; i < next_tasks.size(); ++i) {
		IThreadedTask *task = next_tasks[i];
#ifdef DEBUG_ENABLED
		for (unsigned int j = i + 1; j < next_tasks.size(); ++j) {
			// Cannot add twice the same task
			ZN_ASSERT(next_tasks[j] != task);
		}
#endif
		_next_tasks[i] = task;
	}
}

AsyncDependencyTracker::~AsyncDependencyTracker() {
	// If we get to destroy tasks from here, it means we aborted. They were not scheduled so we still have
	// ownership on them, so we have to clean them up.
	for (auto it = _next_tasks.begin(); it != _next_tasks.end(); ++it) {
		IThreadedTask *task = *it;
		// TODO Might want to allow customizing that, maybe calling a `->dispose()` function instead?
		ZN_DELETE(task);
	}
}

void AsyncDependencyTracker::set_count(int count) {
	ZN_ASSERT_MSG(_count_was_set == false, "Count must not be set twice");
	ZN_ASSERT_MSG(_tasks_have_started == false, "Count must not be set after scheduling tasks");
	_count = count;
	_count_was_set = true;
}

void AsyncDependencyTracker::post_complete() {
	_tasks_have_started = true;
	// Note, this class only allows decrementing this counter down to zero
	ZN_ASSERT_RETURN_MSG(_count > 0, "post_complete() called more times than expected");
	ZN_ASSERT_RETURN_MSG(_aborted == false, "post_complete() called after abortion");
	--_count;
	if (_count == 0 && _next_tasks.size() > 0) {
		ZN_ASSERT_RETURN(_next_tasks_schedule_callback != nullptr);
		_next_tasks_schedule_callback(to_span(_next_tasks));
		// Clearing tasks because once they are scheduled we no longer have ownership on them.
		_next_tasks.clear();
	}
	// The idea of putting next tasks inside this class instead of the tasks directly,
	// is because it would require such tasks to do the job, but also because when waiting for multiple tasks,
	// which one has ownership is fuzzy. It could be any of them that finish last.
	// Putting next tasks in the tracker instead has a clear unique ownership.
}

} // namespace zylann
