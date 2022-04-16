#ifndef ZYLANN_PROGRESSIVE_TASK_RUNNER_H
#define ZYLANN_PROGRESSIVE_TASK_RUNNER_H

#include <cstdint>
#include <queue>

namespace zylann {

// TODO It would be really nice if Godot4 Vulkan buffer deallocation was better optimized.
// This is originally to workaround the terribly slow Vulkan buffer deallocation in Godot4.
// It happens on the main thread and causes deferred stutters when a terrain contains a lot of chunks
// and the camera moves fast.
// I hate this workaround because it feels like we are almost not in control of a stable framerate.
// "Make less meshes" is not enough, if it can't be dynamically adressed.

class IProgressiveTask {
public:
	virtual ~IProgressiveTask() {}
	virtual void run() = 0;
};

// Runs a certain amount of tasks per frame such that all tasks should be completed in N seconds.
// This has the effect of spreading the load over time and avoids CPU spikes.
// This can be used in place of a time-slicing runner when the duration of tasks cannot be used as a cost metric.
// This is the case of tasks that defer their workload to another system to run later. It is far from perfect though,
// and is a last resort solution when optimization and threading are not possible.
// Such tasks may preferably not require low latency in the game,
// because they will likely run a bit later than a time-sliced task.
class ProgressiveTaskRunner {
public:
	~ProgressiveTaskRunner();

	void push(IProgressiveTask *task);
	void process();
	void flush();
	unsigned int get_pending_count() const;

private:
	static const unsigned int MIN_COUNT = 4;
	static const unsigned int COMPLETION_TIME_MSEC = 500;

	std::queue<IProgressiveTask *> _tasks;
	unsigned int _dequeue_count = MIN_COUNT;
	int64_t _last_process_time_msec = 0;
};

} // namespace zylann

#endif // ZYLANN_PROGRESSIVE_TASK_RUNNER_H
