#include "voxel_mesh_block_update_info.h"

namespace zylann::voxel {

Vector3i VoxelMeshBlockUpdateInfo::get_grid_position() const {
	return position;
}

Vector3 VoxelMeshBlockUpdateInfo::get_local_position() const {
	return position * block_size;
}

// int VoxelMeshBlockUpdateInfo::get_lod_index() const {
// 	return lod_index;
// }

bool VoxelMeshBlockUpdateInfo::is_first() const {
	return first_load;
}

Ref<Mesh> VoxelMeshBlockUpdateInfo::get_mesh() const {
	return mesh;
}

void VoxelMeshBlockUpdateInfo::_bind_methods() {
	using Self = VoxelMeshBlockUpdateInfo;

	ClassDB::bind_method(D_METHOD("get_grid_position"), &Self::get_grid_position);
	ClassDB::bind_method(D_METHOD("get_local_position"), &Self::get_local_position);
	// ClassDB::bind_method(D_METHOD("get_lod_index"), &Self::get_lod_index);
	ClassDB::bind_method(D_METHOD("is_first"), &Self::is_first);
	ClassDB::bind_method(D_METHOD("get_mesh"), &Self::get_mesh);
}

} // namespace zylann::voxel
