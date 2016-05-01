#include "voxel_library.h"

VoxelLibrary::VoxelLibrary() : Reference(), _atlas_size(1) {

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

void VoxelLibrary::_bind_methods() {

    ObjectTypeDB::bind_method(_MD("create_voxel:Voxel", "id", "name"), &VoxelLibrary::create_voxel);
    ObjectTypeDB::bind_method(_MD("set_atlas_size", "square_size"), &VoxelLibrary::set_atlas_size);

}

