#include "voxel_library.h"

VoxelLibrary::VoxelLibrary() : Reference(), _atlas_size(1) {
	// Defaults
	create_voxel(0, "air")->set_transparent(true);
	create_voxel(1, "solid")->set_transparent(false)->set_cube_geometry();
}

VoxelLibrary::~VoxelLibrary() {
	for (unsigned int i = 0; i < MAX_VOXEL_TYPES; ++i) {
		if (_voxel_types[i].is_valid()) {
			_voxel_types[i]->set_library_ptr(NULL);
		}
	}
}

void VoxelLibrary::set_atlas_size(int s) {
	ERR_FAIL_COND(s <= 0);
	_atlas_size = s;
}

Ref<Voxel> VoxelLibrary::create_voxel(int id, String name) {
	ERR_FAIL_COND_V(id < 0 || id >= MAX_VOXEL_TYPES, Ref<Voxel>());
	Ref<Voxel> voxel(memnew(Voxel));
	voxel->set_library_ptr(this);
	voxel->set_id(id);
	voxel->set_name(name);
	_voxel_types[id] = voxel;
	return voxel;
}

Ref<Voxel> VoxelLibrary::_get_voxel_bind(int id) {
	ERR_FAIL_COND_V(id < 0 || id >= MAX_VOXEL_TYPES, Ref<Voxel>());
	return _voxel_types[id];
}

void VoxelLibrary::_bind_methods() {

	ClassDB::bind_method(D_METHOD("create_voxel:Voxel", "id", "name"), &VoxelLibrary::create_voxel);
	ClassDB::bind_method(D_METHOD("get_voxel", "id"), &VoxelLibrary::_get_voxel_bind);

	ClassDB::bind_method(D_METHOD("set_atlas_size", "square_size"), &VoxelLibrary::set_atlas_size);

}

