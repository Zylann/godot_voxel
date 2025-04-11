#ifndef VOXEL_RAYCAST_FUNCS_H
#define VOXEL_RAYCAST_FUNCS_H

#include "../meshers/voxel_mesher.h"
#include "../util/godot/core/transform_3d.h"
#include "../util/godot/core/vector3.h"
#include "voxel_raycast_result.h"

namespace zylann::voxel {

class VoxelData;
class VoxelMesherBlocky;

Ref<VoxelRaycastResult> raycast_sdf(
		const VoxelData &voxel_data,
		const Vector3 ray_origin,
		const Vector3 ray_dir,
		const float max_distance,
		const uint8_t binary_search_iterations,
		const bool normal_enabled
);

Ref<VoxelRaycastResult> raycast_blocky(
		const VoxelData &voxel_data,
		const VoxelMesherBlocky &mesher,
		const Vector3 ray_origin,
		const Vector3 ray_dir,
		const float max_distance,
		const uint32_t p_collision_mask
);

Ref<VoxelRaycastResult> raycast_nonzero(
		const VoxelData &voxel_data,
		const Vector3 ray_origin,
		const Vector3 ray_dir,
		const float max_distance,
		const uint8_t p_channel
);

Ref<VoxelRaycastResult> raycast_generic(
		const VoxelData &voxel_data,
		const Ref<VoxelMesher> mesher,
		const Vector3 ray_origin,
		const Vector3 ray_dir,
		const float max_distance,
		const uint32_t p_collision_mask,
		const uint8_t binary_search_iterations,
		const bool normal_enabled
);

Ref<VoxelRaycastResult> raycast_generic_world(
		const VoxelData &voxel_data,
		const Ref<VoxelMesher> mesher,
		const Transform3D &to_world,
		const Vector3 ray_origin_world,
		const Vector3 ray_dir_world,
		const float max_distance_world,
		const uint32_t p_collision_mask,
		const uint8_t binary_search_iterations,
		const bool normal_enabled
);

} // namespace zylann::voxel

#endif // VOXEL_RAYCAST_FUNCS_H
