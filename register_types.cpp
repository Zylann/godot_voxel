#include "register_types.h"
#include "voxel_buffer.h"
#include "voxel_mesher.h"
#include "voxel_library.h"
#include "voxel_map.h"
#include "voxel_terrain.h"
#include "voxel_provider_test.h"
#include "voxel_provider_image.h"
#include "voxel_mesher_smooth.h"

void register_voxel_types() {

	ClassDB::register_class<Voxel>();
	ClassDB::register_class<VoxelBuffer>();
	ClassDB::register_class<VoxelMesher>();
	ClassDB::register_class<VoxelLibrary>();
	ClassDB::register_class<VoxelMap>();
	ClassDB::register_class<VoxelTerrain>();
	ClassDB::register_class<VoxelProvider>();
	ClassDB::register_class<VoxelProviderTest>();
	ClassDB::register_class<VoxelProviderImage>();
	ClassDB::register_class<VoxelMesherSmooth>();

}

void unregister_voxel_types() {

}

