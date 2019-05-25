#include "register_types.h"
#include "meshers/blocky/voxel_mesher_blocky.h"
#include "meshers/dmc/voxel_mesher_dmc.h"
#include "meshers/transvoxel/voxel_mesher_transvoxel.h"
#include "providers/voxel_provider_image.h"
#include "providers/voxel_provider_noise.h"
#include "providers/voxel_provider_test.h"
#include "terrain/voxel_box_mover.h"
#include "terrain/voxel_lod_terrain.h"
#include "terrain/voxel_map.h"
#include "terrain/voxel_terrain.h"
#include "voxel_buffer.h"
#include "voxel_isosurface_tool.h"
#include "voxel_library.h"

void register_voxel_types() {

	// Storage
	ClassDB::register_class<VoxelBuffer>();
	ClassDB::register_class<VoxelMap>();

	// Voxel types
	ClassDB::register_class<Voxel>();
	ClassDB::register_class<VoxelLibrary>();

	// Nodes
	ClassDB::register_class<VoxelTerrain>();
	ClassDB::register_class<VoxelLodTerrain>();

	// Providers
	ClassDB::register_class<VoxelStream>();
	ClassDB::register_class<VoxelStreamTest>();
	ClassDB::register_class<VoxelStreamImage>();
	ClassDB::register_class<VoxelStreamNoise>();

	// Helpers
	ClassDB::register_class<VoxelBoxMover>();
	ClassDB::register_class<VoxelIsoSurfaceTool>();

	// Meshers
	ClassDB::register_class<VoxelMesher>();
	ClassDB::register_class<VoxelMesherBlocky>();
	ClassDB::register_class<VoxelMesherTransvoxel>();
	ClassDB::register_class<VoxelMesherDMC>();
}

void unregister_voxel_types() {
}
