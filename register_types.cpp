#include "register_types.h"
#include "voxel_buffer.h"
#include "voxel_mesher.h"
#include "voxel_library.h"
#include "voxel_map.h"
#include "voxel_terrain.h"

void register_voxel_types() {

	ObjectTypeDB::register_type<Voxel>();
	ObjectTypeDB::register_type<VoxelBuffer>();
	ObjectTypeDB::register_type<VoxelMesher>();
	ObjectTypeDB::register_type<VoxelLibrary>();
	ObjectTypeDB::register_type<VoxelMap>();
	ObjectTypeDB::register_type<VoxelTerrain>();
	ObjectTypeDB::register_type<VoxelProvider>();
	ObjectTypeDB::register_type<VoxelProviderTest>();

}

void unregister_voxel_types() {

}

