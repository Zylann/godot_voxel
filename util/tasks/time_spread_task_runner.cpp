#include "time_spread_task_runner.h"
#include "../profiling.h"

#include <core/os/memory.h>
#include <core/os/time.h>

namespace zylann {

TimeSpreadTaskRunner::~TimeSpreadTaskRunner() {
	flush();
}

void TimeSpreadTaskRunner::push(ITimeSpreadTask *task) {
	MutexLock lock(_tasks_mutex);
	_tasks.push(task);
}

void TimeSpreadTaskRunner::push(Span<ITimeSpreadTask *> tasks) {
	MutexLock lock(_tasks_mutex);
	for (unsigned int i = 0; i < tasks.size(); ++i) {
		_tasks.push(tasks[i]);
	}
}

void TimeSpreadTaskRunner::process(uint64_t time_budget_usec) {
	VOXEL_PROFILE_SCOPE();
	const Time &time = *Time::get_singleton();

	static thread_local std::vector<ITimeSpreadTask *> tls_postponed_tasks;
	CRASH_COND(tls_postponed_tasks.size() > 0);

	const uint64_t time_before = time.get_ticks_usec();

	// Do at least one task
	do {
		ITimeSpreadTask *task;
		{
			MutexLock lock(_tasks_mutex);
			if (_tasks.size() == 0) {
				break;
			}
			task = _tasks.front();
			_tasks.pop();
		}

		TimeSpreadTaskContext ctx;
		task->run(ctx);

		if (ctx.postpone) {
			tls_postponed_tasks.push_back(task);
		} else {
			// TODO Call recycling function instead?
			memdelete(task);
		}

	} while (time.get_ticks_usec() - time_before < time_budget_usec);

	if (tls_postponed_tasks.size() > 0) {
		push(to_span(tls_postponed_tasks));
		tls_postponed_tasks.clear();
	}
}

void TimeSpreadTaskRunner::flush() {
	// Note, it is assumed no other threads can push tasks anymore.
	// It is up to the caller to stop them before flushing.
	while (get_pending_count() != 0) {
		process(100);
		// Sleep?
	}
}

unsigned int TimeSpreadTaskRunner::get_pending_count() const {
	MutexLock lock(_tasks_mutex);
	return _tasks.size();
}

} // namespace zylann
