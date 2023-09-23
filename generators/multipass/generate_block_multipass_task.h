#ifndef VOXEL_GENERATE_BLOCK_MULTIPASS_TASK_H
#define VOXEL_GENERATE_BLOCK_MULTIPASS_TASK_H

#include "../../util/tasks/threaded_task.h"
#include "voxel_generator_multipass.h"

namespace zylann::voxel {

// TODO Alternative: sliding-box pyramid
// This builds on top of VoxelViewers. Have concentric sliding boxes around viewers instead of just one. Each one
// requests blocks at a given level. Tasks still have to check if neighbors are available and get postponed if they are
// found to be loading, and cancelled otherwise.
// However it still means that initial loading and teleports will be quite rough due to cubic bunches of tasks spinning
// spamming the map until they get their dependencies, unless we find a way to pipeline them in a way that minimizes
// this.
// We need to test to see what actually happens, considering tasks are sorted and only a handful of them can run at any
// given time.
// We could pipeline tasks if sliding boxes change all at the same time: we calculate all diffs, then
// execute all tasks of each diff serially (while tasks within a diff get done in parallel). It seems it would induce
// some latency. Need to research.
// We could mess with the sliding boxes themselves, like shrinking them down to a few
// blocks and gradually increasing them back. But the question becomes how long should should the transition be.

// First experimental implementation of executing a generation pass on blocks with neighbor dependencies.
// It naively attempts to generate a single block or column of blocks. It checks first all main blocks and neighbors
// that could get modified in the process. If any aren't loaded to the required level, sub-tasks are spawned to load
// them, the current task gets piped after those, and so on until everything is ready to generate the requestd blocks.
class GenerateBlockMultipassTask : public IThreadedTask {
public:
	GenerateBlockMultipassTask(Vector3i p_block_position, uint8_t p_block_size, uint8_t p_subpass_index,
			Ref<VoxelGeneratorMultipass> p_generator,
			// When the current task finishes, it will decrement the given counter, and return control to the following
			// caller task when the counter reaches 0.
			IThreadedTask *p_caller, std::shared_ptr<std::atomic_int> p_caller_dependency_count);

	~GenerateBlockMultipassTask();

	const char *get_debug_name() const override {
		return "GenerateBlockMultipassTask";
	}

	void run(ThreadedTaskContext &ctx) override;

	// TODO Implement priority
	// TaskPriority get_priority() {}

	// TODO Implement cancellation, not trivial since we use nested tasks
	// bool is_cancelled() {}

private:
	inline void return_to_caller();

	Vector3i _block_position;
	uint8_t _block_size;
	uint8_t _subpass_index;
	Ref<VoxelGeneratorMultipass> _generator;
	// Task to execute when `caller_task_dependency_counter` reaches zero.
	// This means the current task was spawned by this one to compute a dependency.
	IThreadedTask *_caller_task = nullptr;
	// Counter to decrement when the task finishes with an outcome equivalent to "the requested block has been
	// processed".
	std::shared_ptr<std::atomic_int> _caller_task_dependency_counter;

	bool _debug_has_requested_deps = false;
	// struct DebugRequested {
	// 	Vector3i pos;
	// 	int pass_index;
	// 	FixedArray<uint16_t, 4> iterations;
	// };
	// std::vector<DebugRequested> _debug_requested_deps;
};

} // namespace zylann::voxel

#endif // VOXEL_GENERATE_BLOCK_MULTIPASS_TASK_H
