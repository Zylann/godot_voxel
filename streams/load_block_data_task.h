#ifndef LOAD_BLOCK_DATA_TASK_H
#define LOAD_BLOCK_DATA_TASK_H

#include "../engine/ids.h"
#include "../engine/priority_dependency.h"
#include "../engine/streaming_dependency.h"
#include "../util/memory/memory.h"
#include "../util/tasks/threaded_task.h"

namespace zylann::voxel {

class VoxelData;

class LoadBlockDataTask : public IThreadedTask {
public:
	LoadBlockDataTask(VolumeID p_volume_id, Vector3i p_block_pos, uint8_t p_lod, uint8_t p_block_size,
			bool p_request_instances, std::shared_ptr<StreamingDependency> p_stream_dependency,
			PriorityDependency p_priority_dependency, bool generate_cache_data, bool generator_use_gpu,
			const std::shared_ptr<VoxelData> &vdata, TaskCancellationToken cancellation_token);

	~LoadBlockDataTask();

	const char *get_debug_name() const override {
		return "LoadBlockData";
	}

	void run(ThreadedTaskContext &ctx) override;
	TaskPriority get_priority() override;
	bool is_cancelled() override;
	void apply_result() override;

	static int debug_get_running_count();

private:
	PriorityDependency _priority_dependency;
	std::shared_ptr<VoxelBuffer> _voxels;
	UniquePtr<InstanceBlockData> _instances;
	Vector3i _position; // In data blocks of the specified lod
	VolumeID _volume_id;
	uint8_t _lod_index;
	uint8_t _block_size;
	bool _has_run = false;
	bool _too_far = false;
	bool _request_instances = false;
	// bool _request_voxels = false;
	bool _max_lod_hint = false;
	bool _generate_cache_data = true;
	bool _requested_generator_task = false;
	bool _generator_use_gpu = false;
	std::shared_ptr<StreamingDependency> _stream_dependency;
	std::shared_ptr<VoxelData> _voxel_data;
	TaskCancellationToken _cancellation_token;
};

} // namespace zylann::voxel

#endif // LOAD_BLOCK_DATA_TASK_H
