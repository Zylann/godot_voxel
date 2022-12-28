#include "progressive_task_runner.h"
#include "../errors.h"
#include "../math/funcs.h"

#include "../godot/classes/time.h"

namespace zylann {

ProgressiveTaskRunner::~ProgressiveTaskRunner() {
	flush();
	ZN_ASSERT_RETURN_MSG(_tasks.size() == 0, "Tasks got created in destructors?");
}

void ProgressiveTaskRunner::push(IProgressiveTask *task) {
	ZN_ASSERT_RETURN(task != nullptr);
	_tasks.push(task);
}

void ProgressiveTaskRunner::process() {
	const int64_t now_msec = Time::get_singleton()->get_ticks_msec();
	const int64_t delta_msec = now_msec - _last_process_time_msec;
	_last_process_time_msec = now_msec;
	ZN_ASSERT_RETURN(delta_msec >= 0);

	// The goal is to dequeue everything in S seconds.
	// So if we have N tasks and `process` is called F times per second, we must dequeue N / (S * F) tasks.
	// Or put it another way, if we call `process` every D seconds, we must dequeue (D * N) / S tasks.
	// We make sure a minimum amount is run so it cannot be stuck at 0.
	// As the number of pending tasks decreases, we want to keep running the highest amount we calculated.
	// we reset when we are done.

	_dequeue_count = math::max(int64_t(_dequeue_count), (int64_t(_tasks.size()) * delta_msec) / COMPLETION_TIME_MSEC);
	_dequeue_count = math::min(_dequeue_count, math::max(MIN_COUNT, static_cast<unsigned int>(_tasks.size())));

	unsigned int count = _dequeue_count;
	while (_tasks.size() > 0 && count > 0) {
		IProgressiveTask *task = _tasks.front();
		_tasks.pop();
		task->run();
		// TODO Call recycling function instead?
		memdelete(task);
		--count;
	}
}

void ProgressiveTaskRunner::flush() {
	while (!_tasks.empty()) {
		IProgressiveTask *task = _tasks.front();
		_tasks.pop();
		task->run();
		memdelete(task);
	}
}

unsigned int ProgressiveTaskRunner::get_pending_count() const {
	return _tasks.size();
}

} // namespace zylann
