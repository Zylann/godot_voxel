#ifndef VOXEL_MESH_BLOCK_VT_H
#define VOXEL_MESH_BLOCK_VT_H

#include "../../util/godot/classes/material.h"
#include "../voxel_mesh_block.h"

namespace zylann::voxel {

// Stores mesh and collider for one chunk of `VoxelLodTerrain`.
// It doesn't store voxel data, because it may be using different block size, or different data structure.
class VoxelMeshBlockVT : public VoxelMeshBlock {
public:
	// TODO This is now only relevant for `VoxelTerrain`
	enum MeshState {
		MESH_NEVER_UPDATED = 0, // TODO Redundant with MESH_NEED_UPDATE?
		MESH_UP_TO_DATE,
		MESH_NEED_UPDATE, // The mesh is out of date but was not yet scheduled for update
		MESH_UPDATE_NOT_SENT, // The mesh is out of date and was scheduled for update, but no request have been sent yet
		MESH_UPDATE_SENT // The mesh is out of date, and an update request was sent, pending response
	};

	RefCount mesh_viewers;
	RefCount collision_viewers;

	VoxelMeshBlockVT(const Vector3i bpos, unsigned int size) : VoxelMeshBlock(bpos) {
		_position_in_voxels = bpos * size;
	}

	void set_mesh_state(MeshState ms) {
		_mesh_state = ms;
	}

	MeshState get_mesh_state() const {
		return _mesh_state;
	}

	void set_material_override(Ref<Material> material) {
		// Can be invalid if the mesh is empty, we don't create instances for empty meshes
		if (_mesh_instance.is_valid()) {
			_mesh_instance.set_material_override(material);
		}
	}

private:
	MeshState _mesh_state = MESH_NEVER_UPDATED;
};

} // namespace zylann::voxel

#endif // VOXEL_MESH_BLOCK_VT_H
