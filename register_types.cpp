#include "register_types.h"
#include "edition/voxel_tool.h"
#include "edition/voxel_tool_terrain.h"
#include "editor/editor_plugin.h"
#include "editor/graph/voxel_graph_editor_plugin.h"
#include "editor/terrain/voxel_terrain_editor_plugin.h"
#include "generators/graph/voxel_generator_graph.h"
#include "generators/graph/voxel_graph_node_db.h"
#include "generators/voxel_generator_flat.h"
#include "generators/voxel_generator_heightmap.h"
#include "generators/voxel_generator_image.h"
#include "generators/voxel_generator_noise.h"
#include "generators/voxel_generator_noise_2d.h"
#include "generators/voxel_generator_waves.h"
#include "meshers/blocky/voxel_library.h"
#include "meshers/blocky/voxel_mesher_blocky.h"
#include "meshers/dmc/voxel_mesher_dmc.h"
#include "meshers/transvoxel/voxel_mesher_transvoxel.h"
#include "streams/voxel_stream_block_files.h"
#include "streams/voxel_stream_file.h"
#include "streams/voxel_stream_region_files.h"
#include "terrain/voxel_box_mover.h"
#include "terrain/voxel_lod_terrain.h"
#include "terrain/voxel_map.h"
#include "terrain/voxel_terrain.h"
#include "voxel_buffer.h"
#include "voxel_memory_pool.h"
#include "voxel_string_names.h"

void register_voxel_types() {
	VoxelMemoryPool::create_singleton();
	VoxelStringNames::create_singleton();
	VoxelGraphNodeDB::create_singleton();

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
	ClassDB::register_class<VoxelStreamFile>();
	ClassDB::register_class<VoxelStreamBlockFiles>();
	ClassDB::register_class<VoxelStreamRegionFiles>();

	// Generators
	ClassDB::register_class<VoxelGenerator>();
	ClassDB::register_class<VoxelGeneratorFlat>();
	ClassDB::register_class<VoxelGeneratorWaves>();
	ClassDB::register_class<VoxelGeneratorHeightmap>();
	ClassDB::register_class<VoxelGeneratorImage>();
	ClassDB::register_class<VoxelGeneratorNoise2D>();
	ClassDB::register_class<VoxelGeneratorNoise>();
	ClassDB::register_class<VoxelGeneratorGraph>();

	// Helpers
	ClassDB::register_class<VoxelBoxMover>();
	ClassDB::register_class<VoxelRaycastResult>();
	ClassDB::register_class<VoxelTool>();
	ClassDB::register_class<VoxelToolTerrain>();
	ClassDB::register_class<VoxelBlockSerializer>();

	// Meshers
	ClassDB::register_class<VoxelMesher>();
	ClassDB::register_class<VoxelMesherBlocky>();
	ClassDB::register_class<VoxelMesherTransvoxel>();
	ClassDB::register_class<VoxelMesherDMC>();

	// Reminder: how to create a singleton accessible from scripts:
	// Engine::get_singleton()->add_singleton(Engine::Singleton("SingletonName",singleton_instance));

#ifdef TOOLS_ENABLED
	VoxelDebug::create_debug_box_mesh();

	EditorPlugins::add_by_type<VoxelGraphEditorPlugin>();
	EditorPlugins::add_by_type<VoxelTerrainEditorPlugin>();
#endif
}

void unregister_voxel_types() {
	unsigned int used_blocks = VoxelMemoryPool::get_singleton()->debug_get_used_blocks();
	if (used_blocks > 0) {
		ERR_PRINT(String("VoxelMemoryPool: {0} memory blocks are still used when unregistering the module. Recycling leak?").format(varray(used_blocks)));
	}

	VoxelMemoryPool::destroy_singleton();
	VoxelStringNames::destroy_singleton();
	VoxelGraphNodeDB::destroy_singleton();

#ifdef TOOLS_ENABLED
	VoxelDebug::free_debug_box_mesh();

	// TODO No remove?
	//EditorPlugins::remove_by_type<VoxelGraphEditorPlugin>();
#endif
}
