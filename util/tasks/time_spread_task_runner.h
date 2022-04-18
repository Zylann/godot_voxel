#ifndef ZYLANN_TIME_SPREAD_TASK_RUNNER_H
#define ZYLANN_TIME_SPREAD_TASK_RUNNER_H

#include "../span.h"
#include "../thread/mutex.h"
#include <cstdint>
#include <queue>

namespace zylann {

struct TimeSpreadTaskContext {
	// If this is set to `true` by a task,
	// it will be re-scheduled to run again, the next time the runner is processed.
	// Otherwise, the task will be destroyed after it runs.
	bool postpone = false;
};

class ITimeSpreadTask {
public:
	virtual ~ITimeSpreadTask() {}
	virtual void run(TimeSpreadTaskContext &ctx) = 0;
};

// Runs tasks in the caller thread, within a time budget per call. Kind of like coroutines.
class TimeSpreadTaskRunner {
public:
	~TimeSpreadTaskRunner();

	// Pushing is thread-safe.
	void push(ITimeSpreadTask *task);
	void push(Span<ITimeSpreadTask *> tasks);

	void process(uint64_t time_budget_usec);
	void flush();
	unsigned int get_pending_count() const;

private:
	// TODO Optimization: naive thread safety. Should be enough for now.
	std::queue<ITimeSpreadTask *> _tasks;
	BinaryMutex _tasks_mutex;
};

} // namespace zylann

#endif // ZYLANN_TIME_SPREAD_TASK_RUNNER_H
