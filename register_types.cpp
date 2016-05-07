#include "register_types.h"
#include "voxel_buffer.h"
#include "voxel_mesher.h"
#include "voxel_library.h"

void register_voxel_types() {

    ObjectTypeDB::register_type<Voxel>();
    ObjectTypeDB::register_type<VoxelBuffer>();
    ObjectTypeDB::register_type<VoxelMesher>();
    ObjectTypeDB::register_type<VoxelLibrary>();

}

void unregister_voxel_types() {

}

