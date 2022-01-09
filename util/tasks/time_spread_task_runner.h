#ifndef ZYLANN_TIME_SPREAD_TASK_RUNNER_H
#define ZYLANN_TIME_SPREAD_TASK_RUNNER_H

#include <cstdint>
#include <queue>

namespace zylann {

class ITimeSpreadTask {
public:
	virtual ~ITimeSpreadTask() {}
	virtual void run() = 0;
};

// Runs tasks in the caller thread, within a time budget per call.
class TimeSpreadTaskRunner {
public:
	~TimeSpreadTaskRunner();

	void push(ITimeSpreadTask *task);
	void process(uint64_t time_budget_usec);
	void flush();
	unsigned int get_pending_count() const;

private:
	std::queue<ITimeSpreadTask *> _tasks;
};

} // namespace zylann

#endif // ZYLANN_TIME_SPREAD_TASK_RUNNER_H
