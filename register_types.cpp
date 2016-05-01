#include "register_types.h"
#include "voxel_buffer.h"
#include "voxel_mesh_builder.h"
#include "voxel_library.h"

void register_voxel_types() {

    ObjectTypeDB::register_type<Voxel>();
    ObjectTypeDB::register_type<VoxelBuffer>();
    ObjectTypeDB::register_type<VoxelMeshBuilder>();
    ObjectTypeDB::register_type<VoxelLibrary>();

}

void unregister_voxel_types() {

}

