#ifndef VOXEL_MESH_BLOCK_UPDATE_INFO_H
#define VOXEL_MESH_BLOCK_UPDATE_INFO_H

#include "../util/godot/classes/mesh.h"

namespace zylann::voxel {

class VoxelMeshBlockUpdateInfo : public Object {
	GDCLASS(VoxelMeshBlockUpdateInfo, Object)

public:
	Vector3i position;
	// uint8_t lod_index = 0;
	bool first_load = false;
	uint8_t block_size = 0;
	Ref<Mesh> mesh;

	Vector3i get_grid_position() const;
	Vector3 get_local_position() const;
	// int get_lod_index() const;
	bool is_first() const;
	Ref<Mesh> get_mesh() const;

private:
	static void _bind_methods();
};

} // namespace zylann::voxel

#endif // VOXEL_MESH_BLOCK_UPDATE_INFO_H
