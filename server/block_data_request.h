#ifndef BLOCK_DATA_REQUEST_H
#define BLOCK_DATA_REQUEST_H

#include "../util/tasks/threaded_task.h"
#include "priority_dependency.h"
#include "streaming_dependency.h"

namespace zylann::voxel {

class BlockDataRequest : public IThreadedTask {
public:
	enum Type { //
		TYPE_LOAD = 0,
		TYPE_SAVE,
		TYPE_FALLBACK_ON_GENERATOR
	};

	// For loading.
	// Only this one needs priority, since we want loading to have low latency.
	BlockDataRequest(uint32_t p_volume_id, Vector3i p_block_pos, uint8_t p_lod, uint8_t p_block_size,
			bool p_request_instances, std::shared_ptr<StreamingDependency> p_stream_dependency,
			PriorityDependency p_priority_dependency);

	// For saving voxels
	BlockDataRequest(uint32_t p_volume_id, Vector3i p_block_pos, uint8_t p_lod, uint8_t p_block_size,
			std::shared_ptr<VoxelBufferInternal> p_voxels, std::shared_ptr<StreamingDependency> p_stream_dependency);

	// For saving instances
	BlockDataRequest(uint32_t p_volume_id, Vector3i p_block_pos, uint8_t p_lod, uint8_t p_block_size,
			std::unique_ptr<InstanceBlockData> p_instances, std::shared_ptr<StreamingDependency> p_stream_dependency);

	~BlockDataRequest();

	void run(ThreadedTaskContext ctx) override;
	int get_priority() override;
	bool is_cancelled() override;
	void apply_result() override;

	static int debug_get_running_count();

private:
	PriorityDependency _priority_dependency;
	std::shared_ptr<VoxelBufferInternal> _voxels;
	std::unique_ptr<InstanceBlockData> _instances;
	Vector3i _position; // In data blocks of the specified lod
	uint32_t _volume_id;
	uint8_t _lod;
	uint8_t _block_size;
	uint8_t _type;
	bool _has_run = false;
	bool _too_far = false;
	bool _request_instances = false;
	bool _request_voxels = false;
	bool _max_lod_hint = false;
	std::shared_ptr<StreamingDependency> _stream_dependency;
	// TODO Find a way to separate save, it doesnt need sorting
};

} // namespace zylann::voxel

#endif // BLOCK_DATA_REQUEST_H
