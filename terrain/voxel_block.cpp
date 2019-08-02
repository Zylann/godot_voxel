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
	destroy_collision();
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

		// Turn on or off collision with visibility
		if (_static_body) {
			if (_static_body->get_child_count() > 0) {
				CollisionShape* cshape = (CollisionShape*)_static_body->get_child(0);
				if (cshape) {
					cshape->set_disabled(!visible);
				}
			}
		}
	}
	_visible = visible;
}

bool VoxelBlock::is_visible() const {
	return _visible;
}

void VoxelBlock::generate_collision(Node* parent) {

	// Destroy any existing collision
	destroy_collision();

	if (! _mesh_instance.is_valid()) {
		return;
	}

	Ref<Shape> shape = _mesh_instance.get_mesh()->create_trimesh_shape();	// memnew() allocation
	if (shape.is_null()) {
		return;
	}

	_static_body = memnew(StaticBody);
	CollisionShape *cshape = memnew(CollisionShape);
	cshape->set_shape(shape);
	_static_body->add_child(cshape);

	// Align the collision shape with the mesh
	_static_body->set_translation(_position_in_voxels.to_vec3());

	// Add the collision to the Node tree
	_parent = parent;
	_parent->add_child(_static_body);
}

void VoxelBlock::destroy_collision() {

	// If a mesh has been created for this block
	if (_static_body != NULL) {

		// If VoxelTerrain child count == 0, then it has been deleted and perhaps everything else has been too
		// If > 0, then Terrain is active and paging, so delete nodes as normal
		if (_parent->get_child_count() > 0) {
			_parent->remove_child(_static_body);

			if (_static_body->get_child_count() > 0) {
				CollisionShape* cshape = (CollisionShape*)_static_body->get_child(0);
				if (cshape) {
					Ref<Shape> shape = cshape->get_shape();	// From create_trimesh_shape()
					memdelete(cshape);
					shape.unref();
				}
			}

			memdelete(_static_body);
			_static_body = NULL;
		}
	}

}
