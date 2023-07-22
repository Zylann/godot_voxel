#ifndef VOXEL_MESH_BLOCK_VT_H
#define VOXEL_MESH_BLOCK_VT_H

#include "../../util/godot/classes/material.h"
#include "../voxel_mesh_block.h"

namespace zylann::voxel {

// Stores mesh and collider for one chunk of `VoxelLodTerrain`.
// It doesn't store voxel data, because it may be using different block size, or different data structure.
//
// Note that such a block can also not contain a mesh, in case voxels in this area do not produce geometry. For example,
// it can be used to check if an area has been loaded (so we *know* that it should or should not have a mesh, as opposed
// to not knowing while threads are still computing the mesh).
class VoxelMeshBlockVT : public VoxelMeshBlock {
public:
	RefCount mesh_viewers;
	RefCount collision_viewers;

	// True if this block is in the update list of `VoxelTerrain`, so multiple edits done before it processes will not
	// add it multiple times
	bool is_in_update_list = false;

	// Will be true if the block has ever been processed by meshing (regardless of there being a mesh or not).
	// This is needed to know if the area is loaded, in terms of collisions. If the game uses voxels directly for
	// collision, it may be a better idea to use `is_area_editable` and not use mesh blocks
	bool is_loaded = false;

	VoxelMeshBlockVT(const Vector3i bpos, unsigned int size) : VoxelMeshBlock(bpos) {
		_position_in_voxels = bpos * size;
	}

	void set_material_override(Ref<Material> material) {
		// Can be invalid if the mesh is empty, we don't create instances for empty meshes
		if (_mesh_instance.is_valid()) {
			_mesh_instance.set_material_override(material);
		}
	}
};

} // namespace zylann::voxel

#endif // VOXEL_MESH_BLOCK_VT_H
