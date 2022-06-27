#ifndef LOAD_BLOCK_DATA_TASK_H
#define LOAD_BLOCK_DATA_TASK_H

#include "../util/memory.h"
#include "../util/tasks/threaded_task.h"
#include "priority_dependency.h"
#include "streaming_dependency.h"

namespace zylann::voxel {

class LoadBlockDataTask : public IThreadedTask {
public:
	LoadBlockDataTask(uint32_t p_volume_id, Vector3i p_block_pos, uint8_t p_lod, uint8_t p_block_size,
			bool p_request_instances, std::shared_ptr<StreamingDependency> p_stream_dependency,
			PriorityDependency p_priority_dependency, bool generate_cache_data);

	~LoadBlockDataTask();

	void run(ThreadedTaskContext ctx) override;
	int get_priority() override;
	bool is_cancelled() override;
	void apply_result() override;

	static int debug_get_running_count();

private:
	PriorityDependency _priority_dependency;
	std::shared_ptr<VoxelBufferInternal> _voxels;
	UniquePtr<InstanceBlockData> _instances;
	Vector3i _position; // In data blocks of the specified lod
	uint32_t _volume_id;
	uint8_t _lod;
	uint8_t _block_size;
	bool _has_run = false;
	bool _too_far = false;
	bool _request_instances = false;
	//bool _request_voxels = false;
	bool _max_lod_hint = false;
	bool _generate_cache_data = true;
	bool _requested_generator_task = false;
	std::shared_ptr<StreamingDependency> _stream_dependency;
};

} // namespace zylann::voxel

#endif // LOAD_BLOCK_DATA_TASK_H
