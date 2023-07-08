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

	std::shared_ptr<VoxelBufferInternal> voxels;
	Vector3i position;
	VolumeID volume_id;
	uint8_t lod_index;
	uint8_t block_size;
	bool has_run = false;
	bool too_far = false;
	bool max_lod_hint = false;
	bool drop_beyond_max_distance = true;
	bool use_gpu = false;
	PriorityDependency priority_dependency;
	std::shared_ptr<StreamingDependency> stream_dependency;
	std::shared_ptr<VoxelData> data;
	std::shared_ptr<AsyncDependencyTracker> tracker;

private:
	void run_gpu_task(zylann::ThreadedTaskContext &ctx);
	void run_gpu_conversion();
	void run_cpu_generation();
	void run_stream_saving_and_finish();

	uint8_t _stage = 0;
	std::vector<GenerateBlockGPUTaskResult> _gpu_generation_results;
};

} // namespace voxel
} // namespace zylann

#endif // GENERATE_BLOCK_TASK_H
