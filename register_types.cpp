#include "register_types.h"
#include "edition/voxel_tool.h"
#include "meshers/blocky/voxel_mesher_blocky.h"
#include "meshers/dmc/voxel_mesher_dmc.h"
#include "meshers/transvoxel/voxel_mesher_transvoxel.h"
#include "streams/voxel_stream_block_files.h"
#include "streams/voxel_stream_file.h"
#include "streams/voxel_stream_heightmap.h"
#include "streams/voxel_stream_image.h"
#include "streams/voxel_stream_noise.h"
#include "streams/voxel_stream_noise_2d.h"
#include "streams/voxel_stream_region_files.h"
#include "streams/voxel_stream_test.h"
#include "terrain/voxel_box_mover.h"
#include "terrain/voxel_lod_terrain.h"
#include "terrain/voxel_map.h"
#include "terrain/voxel_terrain.h"
#include "voxel_buffer.h"
#include "voxel_library.h"
#include "voxel_memory_pool.h"
#include "voxel_string_names.h"

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

	// Streams
	ClassDB::register_class<VoxelStream>();
	ClassDB::register_class<VoxelStreamTest>();
	ClassDB::register_class<VoxelStreamHeightmap>();
	ClassDB::register_class<VoxelStreamImage>();
	ClassDB::register_class<VoxelStreamNoise>();
	ClassDB::register_class<VoxelStreamNoise2D>();
	ClassDB::register_class<VoxelStreamFile>();
	ClassDB::register_class<VoxelStreamBlockFiles>();
	ClassDB::register_class<VoxelStreamRegionFiles>();

	// Helpers
	ClassDB::register_class<VoxelBoxMover>();
	ClassDB::register_class<VoxelRaycastResult>();
	ClassDB::register_class<VoxelTool>();

	// Meshers
	ClassDB::register_class<VoxelMesher>();
	ClassDB::register_class<VoxelMesherBlocky>();
	ClassDB::register_class<VoxelMesherTransvoxel>();
	ClassDB::register_class<VoxelMesherDMC>();

	VoxelMemoryPool::create_singleton();
	VoxelStringNames::create_singleton();

#ifdef TOOLS_ENABLED
	VoxelDebug::create_debug_box_mesh();
#endif
}

void unregister_voxel_types() {

	unsigned int used_blocks = VoxelMemoryPool::get_singleton()->debug_get_used_blocks();
	if (used_blocks > 0) {
		ERR_PRINT(String("VoxelMemoryPool: {0} memory blocks are still used when unregistering the module. Recycling leak?").format(varray(used_blocks)));
	}
	VoxelMemoryPool::destroy_singleton();

	VoxelStringNames::destroy_singleton();

#ifdef TOOLS_ENABLED
	VoxelDebug::free_debug_box_mesh();
#endif
}
