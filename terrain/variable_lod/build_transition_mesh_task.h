#ifndef VOXEL_BUILD_TRANSITION_MESH_TASK_H
#define VOXEL_BUILD_TRANSITION_MESH_TASK_H

#include "../../meshers/voxel_mesher.h"
#include "../../util/tasks/time_spread_task_runner.h"

namespace zylann::voxel {

class VoxelLodTerrain;
class VoxelMeshBlockVLT;

// This mess is because Godot takes a non-neglibeable base amount of time to build meshe, regardless of their size
// (About 50us, so 300us for all 6 transitions of one block). Transition meshes are rarely needed immediately, so we
// defer them in some cases.
class BuildTransitionMeshTask : public ITimeSpreadTask {
public:
	void run(TimeSpreadTaskContext &ctx) override;

	VoxelLodTerrain *volume;
	uint32_t volume_id;
	uint32_t mesh_flags;
	uint8_t primitive_type;
	uint8_t side;
	std::vector<VoxelMesher::Output::Surface> mesh_data;
	VoxelMeshBlockVLT *block = nullptr; // To be set only when scheduled
};

} // namespace zylann::voxel

#endif // VOXEL_BUILD_TRANSITION_MESH_TASK_H
