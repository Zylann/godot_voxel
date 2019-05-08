#include "voxel_block.h"

// Helper
VoxelBlock *VoxelBlock::create(Vector3i bpos, Ref<VoxelBuffer> buffer, unsigned int size, unsigned int p_lod_index) {
	const int bs = size;
	ERR_FAIL_COND_V(buffer.is_null(), NULL);
	ERR_FAIL_COND_V(buffer->get_size() != Vector3i(bs, bs, bs), NULL);

	VoxelBlock *block = memnew(VoxelBlock);
	block->pos = bpos;
	block->lod_index = p_lod_index;
	block->_position_in_voxels = bpos * (size << p_lod_index);

	block->voxels = buffer;
	return block;
}

VoxelBlock::VoxelBlock() :
		voxels(NULL) {
}

VoxelBlock::~VoxelBlock() {
	VisualServer &vs = *VisualServer::get_singleton();

	if (_mesh_instance.is_valid()) {
		vs.free(_mesh_instance);
		_mesh_instance = RID();
	}
}

void VoxelBlock::set_mesh(Ref<Mesh> mesh, Ref<World> world) {

	VisualServer &vs = *VisualServer::get_singleton();

	if (mesh.is_valid()) {

		if (_mesh_instance.is_valid() == false) {
			// Create instance if it doesn't exist
			ERR_FAIL_COND(world.is_null());
			_mesh_instance = vs.instance_create();
			vs.instance_set_scenario(_mesh_instance, world->get_scenario());
		}

		vs.instance_set_base(_mesh_instance, mesh.is_valid() ? mesh->get_rid() : RID());

		Transform local_transform(Basis(), _position_in_voxels.to_vec3());
		vs.instance_set_transform(_mesh_instance, local_transform);
		// TODO The day VoxelTerrain becomes a Spatial, this transform will need to be updatable separately

	} else {

		if (_mesh_instance.is_valid()) {
			// Delete instance if it exists
			vs.free(_mesh_instance);
			_mesh_instance = RID();
		}
	}

	_mesh = mesh;
	++_mesh_update_count;

	//	if(_mesh_update_count > 1) {
	//		print_line(String("Block {0} was updated {1} times").format(varray(pos.to_vec3(), _mesh_update_count)));
	//	}
}

bool VoxelBlock::has_mesh() const {
	return _mesh.is_valid();
}

void VoxelBlock::enter_world(World *world) {
	if (_mesh_instance.is_valid()) {
		VisualServer &vs = *VisualServer::get_singleton();
		vs.instance_set_scenario(_mesh_instance, world->get_scenario());
	}
}

void VoxelBlock::exit_world() {
	if (_mesh_instance.is_valid()) {
		VisualServer &vs = *VisualServer::get_singleton();
		vs.instance_set_scenario(_mesh_instance, RID());
	}
}

void VoxelBlock::set_visible(bool visible) {
	if (_mesh_instance.is_valid()) {
		VisualServer &vs = *VisualServer::get_singleton();
		vs.instance_set_visible(_mesh_instance, visible);
	}
	_visible = visible;
}

bool VoxelBlock::is_visible() const {
	return _visible;
}
