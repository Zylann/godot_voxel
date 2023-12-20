#ifndef GENERATE_BLOCK_TASK_H
#define GENERATE_BLOCK_TASK_H

#include "../engine/ids.h"
#include "../engine/priority_dependency.h"
#include "../engine/streaming_dependency.h"
#include "../util/tasks/threaded_task.h"
#include "generate_block_gpu_task.h"

namespace zylann {

class AsyncDependencyTracker;

namespace voxel {

class VoxelData;

// Generic task to procedurally generate a block of voxels in a single pass
class GenerateBlockTask : public IGeneratingVoxelsThreadedTask {
public:
	GenerateBlockTask(const VoxelGenerator::BlockTaskParams &params);
	~GenerateBlockTask();

	const char *get_debug_name() const override {
		return "GenerateBlock";
	}

	void run(ThreadedTaskContext &ctx) override;
	TaskPriority get_priority() override;
	bool is_cancelled() override;
	void apply_result() override;

	void set_gpu_results(std::vector<GenerateBlockGPUTaskResult> &&results) override;

private:
	void run_gpu_task(zylann::ThreadedTaskContext &ctx);
	void run_gpu_conversion();
	void run_cpu_generation();
	void run_stream_saving_and_finish();

	// Not an input, but can be assigned a re-usable instance to avoid allocating one in the task
	std::shared_ptr<VoxelBufferInternal> _voxels;

	Vector3i _position;
	VolumeID _volume_id;
	uint8_t _lod_index;
	uint8_t _block_size;
	bool _drop_beyond_max_distance = true;
	bool _use_gpu = false;
	PriorityDependency _priority_dependency;
	std::shared_ptr<StreamingDependency> _stream_dependency; // For saving generator output
	std::shared_ptr<VoxelData> _data; // Just for modifiers
	std::shared_ptr<AsyncDependencyTracker> _tracker; // For async edits

	bool _has_run = false;
	bool _too_far = false;
	bool _max_lod_hint = false;
	uint8_t _stage = 0;
	std::vector<GenerateBlockGPUTaskResult> _gpu_generation_results;
};

} // namespace voxel
} // namespace zylann

#endif // GENERATE_BLOCK_TASK_H
