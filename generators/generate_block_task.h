#ifndef GENERATE_BLOCK_TASK_H
#define GENERATE_BLOCK_TASK_H

#include "../engine/ids.h"
#include "../engine/priority_dependency.h"
#include "../engine/streaming_dependency.h"
#include "../util/containers/std_vector.h"
#include "../util/tasks/threaded_task.h"

#ifdef VOXEL_ENABLE_GPU
#include "generate_block_gpu_task.h"
#endif

namespace zylann {

class AsyncDependencyTracker;

namespace voxel {

class VoxelData;

// Generic task to procedurally generate a block of voxels in a single pass
class GenerateBlockTask
#ifdef VOXEL_ENABLE_GPU
		: public IGeneratingVoxelsThreadedTask
#else
		: public IThreadedTask
#endif
{
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

#ifdef VOXEL_ENABLE_GPU
	void set_gpu_results(StdVector<GenerateBlockGPUTaskResult> &&results) override;
#endif

private:
#ifdef VOXEL_ENABLE_GPU
	void run_gpu_task(zylann::ThreadedTaskContext &ctx);
	void run_gpu_conversion();
#endif
	void run_cpu_generation();
	void run_stream_saving_and_finish();

	// Not an input, but can be assigned a re-usable instance to avoid allocating one in the task
	std::shared_ptr<VoxelBuffer> _voxels;

	Vector3i _position;
	VoxelFormat _format;
	VolumeID _volume_id;
	uint8_t _lod_index;
	uint8_t _block_size;
	bool _drop_beyond_max_distance = true;
#ifdef VOXEL_ENABLE_GPU
	bool _use_gpu = false;
#endif
	PriorityDependency _priority_dependency;
	std::shared_ptr<StreamingDependency> _stream_dependency; // For saving generator output
	std::shared_ptr<VoxelData> _data; // Just for modifiers
	std::shared_ptr<AsyncDependencyTracker> _tracker; // For async edits
	TaskCancellationToken _cancellation_token;

	bool _has_run = false;
	bool _too_far = false;
	bool _max_lod_hint = false;
#ifdef VOXEL_ENABLE_GPU
	uint8_t _stage = 0;
	StdVector<GenerateBlockGPUTaskResult> _gpu_generation_results;
#endif
};

} // namespace voxel
} // namespace zylann

#endif // GENERATE_BLOCK_TASK_H
