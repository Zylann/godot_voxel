#include "voxel_block.h"
#include <scene/resources/world.h>

// Helper
VoxelBlock *VoxelBlock::create(Vector3i bpos, Ref<VoxelBuffer> buffer, unsigned int size, unsigned int p_lod_index) {
	const int bs = size;
	ERR_FAIL_COND_V(buffer.is_null(), NULL);
	ERR_FAIL_COND_V(buffer->get_size() != Vector3i(bs, bs, bs), NULL);

	VoxelBlock *block = memnew(VoxelBlock);
	block->position = bpos;
	block->lod_index = p_lod_index;
	block->_position_in_voxels = bpos * (size << p_lod_index);
	block->voxels = buffer;

	return block;
}

VoxelBlock::VoxelBlock() {
}

VoxelBlock::~VoxelBlock() {
}

void VoxelBlock::set_mesh(Ref<Mesh> mesh, Ref<World> world) {
	// TODO Don't add mesh instance to the world if it's not visible.
	// I suspect Godot is trying to include invisible mesh instances into the culling process,
	// which is killing performance when LOD is used (i.e many meshes are in pool but hidden)
	// This needs investigation.

	if (mesh.is_valid()) {

		if (!_mesh_instance.is_valid()) {
			// Create instance if it doesn't exist
			ERR_FAIL_COND(world.is_null());
			_mesh_instance.create();
			_mesh_instance.set_world(*world);
		}

		_mesh_instance.set_mesh(mesh);
		_mesh_instance.set_transform(Transform(Basis(), _position_in_voxels.to_vec3()));
		// TODO The day VoxelTerrain becomes a Spatial, this transform will need to be updatable separately

	} else {

		if (_mesh_instance.is_valid()) {
			// Delete instance if it exists
			_mesh_instance.destroy();
		}
	}

	++_mesh_update_count;

	//	if(_mesh_update_count > 1) {
	//		print_line(String("Block {0} was updated {1} times").format(varray(pos.to_vec3(), _mesh_update_count)));
	//	}
}

bool VoxelBlock::has_mesh() const {
	return _mesh_instance.get_mesh().is_valid();
}

void VoxelBlock::set_mesh_state(MeshState ms) {
	_mesh_state = ms;
}

VoxelBlock::MeshState VoxelBlock::get_mesh_state() const {
	return _mesh_state;
}

void VoxelBlock::mark_been_meshed() {
	_has_been_meshed = true;
}

bool VoxelBlock::has_been_meshed() const {
	return _has_been_meshed;
}

void VoxelBlock::set_world(World *world) {
	if (_mesh_instance.is_valid()) {
		_mesh_instance.set_world(world);
	}
}

void VoxelBlock::set_visible(bool visible) {
	if (_mesh_instance.is_valid()) {
		_mesh_instance.set_visible(visible);
	}
	_visible = visible;
}

bool VoxelBlock::is_visible() const {
	return _visible;
}
