#ifndef VOXEL_MESH_BLOCK_TASK_H
#define VOXEL_MESH_BLOCK_TASK_H

#include "../constants/voxel_constants.h"
#include "../storage/voxel_buffer_internal.h"
#include "../util/tasks/threaded_task.h"
#include "meshing_dependency.h"
#include "priority_dependency.h"

namespace zylann::voxel {

// Asynchronous task generating a mesh from voxel blocks and their neighbors, in a particular volume
class MeshBlockTask : public IThreadedTask {
public:
	MeshBlockTask();
	~MeshBlockTask();

	void run(ThreadedTaskContext ctx) override;
	int get_priority() override;
	bool is_cancelled() override;
	void apply_result() override;

	static int debug_get_running_count();

	// 3x3x3 or 4x4x4 grid of voxel blocks.
	FixedArray<std::shared_ptr<VoxelBufferInternal>, constants::MAX_BLOCK_COUNT_PER_REQUEST> blocks;
	// TODO Need to provide format
	//FixedArray<uint8_t, VoxelBufferInternal::MAX_CHANNELS> channel_depths;
	Vector3i position; // In mesh blocks of the specified lod
	uint32_t volume_id;
	uint8_t lod_index = 0;
	uint8_t blocks_count = 0;
	uint8_t data_block_size = 0;
	bool collision_hint = false;
	bool lod_hint = false;
	PriorityDependency priority_dependency;
	std::shared_ptr<MeshingDependency> meshing_dependency;
	std::shared_ptr<VoxelDataLodMap> data;

private:
	bool _has_run = false;
	bool _too_far = false;
	VoxelMesher::Output _surfaces_output;
};

} // namespace zylann::voxel

#endif // VOXEL_MESH_BLOCK_TASK_H
