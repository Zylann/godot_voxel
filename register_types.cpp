#include "register_types.h"
#include "voxel_buffer.h"
#include "voxel_mesh_builder.h"

void register_voxel_types() {

    ObjectTypeDB::register_type<Voxel>();
    ObjectTypeDB::register_type<VoxelBuffer>();
    ObjectTypeDB::register_type<VoxelMeshBuilder>();

}

void unregister_voxel_types() {

}

