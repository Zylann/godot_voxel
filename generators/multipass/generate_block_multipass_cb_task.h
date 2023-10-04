#ifndef VOXEL_GENERATE_BLOCK_MULTIPASS_CB_TASK_H
#define VOXEL_GENERATE_BLOCK_MULTIPASS_CB_TASK_H

#include "../../util/tasks/threaded_task.h"
#include "voxel_generator_multipass_cb.h"

namespace zylann::voxel {

// This task is designed to be scheduled from another.
// Looks up columns in the generator's cache in order to run a pass on a specific column.
// If at least one column isn't found in the map, the task is cancelled, and so should be all its callers.
// Otherwise:
// If a column doesn't fulfills dependency requirements:
//     - If another task is working on that column, the current task is postponed to run later.
//     - Otherwise, a subtask is spawned to work on the dependency.
//       The current task is queued after every subtask spawned this way.
// Otherwise, the task runs the pass, re-schedules its caller, and returns.
//
// One reason to use this pattern instead of "pyramid diffs", is that it can be invoked without assumptions. It will
// return a result if necessary, even if the map is in inconsistent state. We can even decide to override states.
// It actively looks for dependencies, rather than assuming they are loaded by separate logic.
class GenerateBlockMultipassCBTask : public IThreadedTask {
public:
	GenerateBlockMultipassCBTask(Vector2i p_column_position, uint8_t p_block_size, uint8_t p_subpass_index,
			Ref<VoxelGeneratorMultipassCB> p_generator,
			// When the current task finishes, it will decrement the given counter, and return control to the following
			// caller task when the counter reaches 0.
			IThreadedTask *p_caller, std::shared_ptr<std::atomic_int> p_caller_dependency_count);

	~GenerateBlockMultipassCBTask();

	const char *get_debug_name() const override {
		return "GenerateBlockMultipassCBTask";
	}

	void run(ThreadedTaskContext &ctx) override;

	// TODO Base priority off the original GenerateBlockTask?
	// We could avoid computing it by passing its cached value through ThreadedTaskContext
	TaskPriority get_priority() {
		return TaskPriority(0, 0, constants::TASK_PRIORITY_GENERATE_BAND2, 0);
	}

	// Cancellation cannot use this API for now (it would prevent the task from running) because the task must run in
	// order to re-schedule its caller. Eventually we may find a way to integrate this pattern into the framework.
	// bool is_cancelled() {}

private:
	void return_to_caller(bool success);

	Vector2i _column_position;
	uint8_t _block_size;
	uint8_t _subpass_index;
	bool _cancelled = false;
	Ref<VoxelGeneratorMultipassCB> _generator;
	// Task to execute when `caller_task_dependency_counter` reaches zero.
	// This means the current task was spawned by this one to compute a dependency.
	IThreadedTask *_caller_task = nullptr;
	// Counter to decrement when the task finishes with an outcome equivalent to "the requested block has been
	// processed".
	std::shared_ptr<std::atomic_int> _caller_task_dependency_counter;
	GenerateBlockMultipassCBTask *_caller_mp_task = nullptr;
};

} // namespace zylann::voxel

#endif // VOXEL_GENERATE_BLOCK_MULTIPASS_TASK_H
