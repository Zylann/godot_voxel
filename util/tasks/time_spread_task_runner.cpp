#include "time_spread_task_runner.h"
#include "../profiling.h"

#include <core/os/memory.h>
#include <core/os/time.h>

namespace zylann {

TimeSpreadTaskRunner::~TimeSpreadTaskRunner() {
	flush();
}

void TimeSpreadTaskRunner::push(ITimeSpreadTask *task) {
	_tasks.push(task);
}

void TimeSpreadTaskRunner::process(uint64_t time_budget_usec) {
	VOXEL_PROFILE_SCOPE();
	const Time &time = *Time::get_singleton();

	if (_tasks.size() > 0) {
		const uint64_t time_before = time.get_ticks_usec();

		// Do at least one task
		do {
			ITimeSpreadTask *task = _tasks.front();
			_tasks.pop();
			task->run();
			// TODO Call recycling function instead?
			memdelete(task);

		} while (_tasks.size() > 0 && time.get_ticks_usec() - time_before < time_budget_usec);
	}
}

void TimeSpreadTaskRunner::flush() {
	while (!_tasks.empty()) {
		ITimeSpreadTask *task = _tasks.front();
		_tasks.pop();
		task->run();
		memdelete(task);
	}
}

unsigned int TimeSpreadTaskRunner::get_pending_count() const {
	return _tasks.size();
}

} // namespace zylann
