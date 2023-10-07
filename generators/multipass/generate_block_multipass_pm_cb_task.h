#if 0
#ifndef VOXEL_GENERATE_BLOCK_MULTIPASS_PM_CB_TASK_H
#define VOXEL_GENERATE_BLOCK_MULTIPASS_PM_CB_TASK_H

#include "../../util/tasks/threaded_task.h"
#include "voxel_generator_multipass_cb.h"

namespace zylann::voxel {

class GenerateBlockMultipassPMCBTask : public IThreadedTask {
public:
	GenerateBlockMultipassPMCBTask(Vector2i p_column_position, uint8_t p_block_size, uint8_t p_subpass_index,
			Ref<VoxelGeneratorMultipassCB> p_generator, TaskPriority p_priority);

	~GenerateBlockMultipassPMCBTask();

	const char *get_debug_name() const override {
		return "GenerateBlockMultipassPMCBTask";
	}

	void run(ThreadedTaskContext &ctx) override;

	TaskPriority get_priority() override {
		return _priority;
	}

	// TODO Implement cancellation
	// bool is_cancelled() {}

private:
	Vector2i _column_position;
	TaskPriority _priority;
	uint8_t _block_size;
	uint8_t _subpass_index;
	Ref<VoxelGeneratorMultipassCB> _generator;
};

} // namespace zylann::voxel

#endif // VOXEL_GENERATE_BLOCK_MULTIPASS_PM_TASK_H
#endif
