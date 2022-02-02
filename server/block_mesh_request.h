#ifndef VOXEL_BLOCK_MESH_REQUEST_H
#define VOXEL_BLOCK_MESH_REQUEST_H

#include "../constants/voxel_constants.h"
#include "../generators/voxel_generator.h"
#include "../meshers/voxel_mesher.h"
#include "../storage/voxel_buffer_internal.h"
#include "../util/tasks/threaded_task.h"
#include "priority_dependency.h"

namespace zylann::voxel {

// Asynchronous task generating a mesh from voxel blocks and their neighbors, in a particular volume
class BlockMeshRequest : public IThreadedTask {
public:
	struct MeshingDependency {
		Ref<VoxelMesher> mesher;
		Ref<VoxelGenerator> generator;
		bool valid = true;
	};

	BlockMeshRequest();
	~BlockMeshRequest();

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
	uint8_t lod;
	uint8_t blocks_count;
	uint8_t data_block_size;
	PriorityDependency priority_dependency;
	std::shared_ptr<MeshingDependency> meshing_dependency;

private:
	bool _has_run = false;
	bool _too_far = false;
	VoxelMesher::Output _surfaces_output;
};

} // namespace zylann::voxel

#endif // VOXEL_BLOCK_MESH_REQUEST_H
