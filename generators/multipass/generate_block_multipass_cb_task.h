#ifndef VOXEL_GENERATE_BLOCK_MULTIPASS_CB_TASK_H
#define VOXEL_GENERATE_BLOCK_MULTIPASS_CB_TASK_H

#include "../../engine/ids.h"
#include "../../engine/priority_dependency.h"
#include "../../engine/streaming_dependency.h"
#include "../../util/tasks/threaded_task.h"
#include "../voxel_generator.h"

namespace zylann {

class AsyncDependencyTracker;

namespace voxel {

class VoxelData;

class GenerateBlockMultipassCBTask : public IThreadedTask {
public:
	GenerateBlockMultipassCBTask(const VoxelGenerator::BlockTaskParams &params);
	~GenerateBlockMultipassCBTask();

	const char *get_debug_name() const override {
		return "GenerateBlockMultipassCBTask";
	}

	void run(ThreadedTaskContext &ctx) override;
	TaskPriority get_priority() override;
	bool is_cancelled() override;
	void apply_result() override;

	// Not an input, but can be assigned a re-usable instance to avoid allocating one in the task
	std::shared_ptr<VoxelBuffer> voxels;

private:
	void run_cpu_generation();
	void run_stream_saving_and_finish();

	Vector3i _block_position;
	VolumeID _volume_id;
	uint8_t _lod_index;
	uint8_t _block_size;
	bool _drop_beyond_max_distance = true;
	PriorityDependency _priority_dependency;
	std::shared_ptr<StreamingDependency> _stream_dependency; // For saving generator output
	std::shared_ptr<AsyncDependencyTracker> _tracker; // For async edits
	TaskCancellationToken _cancellation_token;

	bool _has_run = false;
	bool _too_far = false;
	uint8_t _stage = 0;
};

} // namespace voxel
} // namespace zylann

#endif // VOXEL_GENERATE_BLOCK_MULTIPASS_CB_TASK_H
