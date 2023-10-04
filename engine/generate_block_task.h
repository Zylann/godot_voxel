#ifndef GENERATE_BLOCK_TASK_H
#define GENERATE_BLOCK_TASK_H

#include "../util/tasks/threaded_task.h"
#include "generate_block_gpu_task.h"
#include "ids.h"
#include "priority_dependency.h"
#include "streaming_dependency.h"

namespace zylann {

class AsyncDependencyTracker;

namespace voxel {

class VoxelData;

class GenerateBlockTask : public IGeneratingVoxelsThreadedTask {
public:
	GenerateBlockTask();
	~GenerateBlockTask();

	const char *get_debug_name() const override {
		return "GenerateBlock";
	}

	void run(ThreadedTaskContext &ctx) override;
	TaskPriority get_priority() override;
	bool is_cancelled() override;
	void apply_result() override;

	void set_gpu_results(std::vector<GenerateBlockGPUTaskResult> &&results) override;

	static int debug_get_running_count();

	// Not an input, but can be assigned a re-usable instance to avoid allocating one in the task
	std::shared_ptr<VoxelBufferInternal> voxels;

	Vector3i position;
	VolumeID volume_id;
	uint8_t lod_index;
	uint8_t block_size;
	bool drop_beyond_max_distance = true;
	bool use_gpu = false;
	PriorityDependency priority_dependency;
	std::shared_ptr<StreamingDependency> stream_dependency; // For saving generator output
	std::shared_ptr<VoxelData> data; // Just for modifiers
	std::shared_ptr<AsyncDependencyTracker> tracker; // For async edits

private:
	void run_gpu_task(zylann::ThreadedTaskContext &ctx);
	void run_gpu_conversion();
	void run_cpu_generation();
	void run_stream_saving_and_finish();

	bool _has_run = false;
	bool _too_far = false;
	bool _max_lod_hint = false;
	uint8_t _stage = 0;
	std::vector<GenerateBlockGPUTaskResult> _gpu_generation_results;
};

} // namespace voxel
} // namespace zylann

#endif // GENERATE_BLOCK_TASK_H
