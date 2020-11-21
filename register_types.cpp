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
#include "generators/voxel_generator_script.h"
#include "generators/voxel_generator_waves.h"
#include "meshers/blocky/voxel_library.h"
#include "meshers/blocky/voxel_mesher_blocky.h"
#include "meshers/cubes/voxel_mesher_cubes.h"
#include "meshers/dmc/voxel_mesher_dmc.h"
#include "meshers/transvoxel/voxel_mesher_transvoxel.h"
#include "storage/voxel_buffer.h"
#include "storage/voxel_memory_pool.h"
#include "streams/vox_loader.h"
#include "streams/voxel_stream_block_files.h"
#include "streams/voxel_stream_file.h"
#include "streams/voxel_stream_region_files.h"
#include "streams/voxel_stream_script.h"
#include "terrain/voxel_box_mover.h"
#include "terrain/voxel_lod_terrain.h"
#include "terrain/voxel_map.h"
#include "terrain/voxel_terrain.h"
#include "terrain/voxel_viewer.h"
#include "util/macros.h"
#include "voxel_string_names.h"
#include <core/engine.h>

#ifdef TOOLS_ENABLED
#include "editor/voxel_debug.h"
#endif

void register_voxel_types() {
	VoxelMemoryPool::create_singleton();
	VoxelStringNames::create_singleton();
	VoxelGraphNodeDB::create_singleton();
	VoxelServer::create_singleton();

	Engine::get_singleton()->add_singleton(Engine::Singleton("VoxelServer", VoxelServer::get_singleton()));

	// TODO Can I prevent users from instancing it? is "register_virtual_class" correct for a class that's not abstract?
	ClassDB::register_class<VoxelServer>();

	ClassDB::register_class<Voxel>();
	ClassDB::register_class<VoxelLibrary>();
	ClassDB::register_class<VoxelColorPalette>();

	// Storage
	ClassDB::register_class<VoxelBuffer>();

	// Nodes
	ClassDB::register_class<VoxelTerrain>();
	ClassDB::register_class<VoxelLodTerrain>();
	ClassDB::register_class<VoxelViewer>();

	// Streams
	ClassDB::register_class<VoxelStream>();
	ClassDB::register_class<VoxelStreamFile>();
	ClassDB::register_class<VoxelStreamBlockFiles>();
	ClassDB::register_class<VoxelStreamRegionFiles>();
	ClassDB::register_class<VoxelStreamScript>();

	// Generators
	ClassDB::register_class<VoxelGenerator>();
	ClassDB::register_class<VoxelGeneratorFlat>();
	ClassDB::register_class<VoxelGeneratorWaves>();
	ClassDB::register_class<VoxelGeneratorHeightmap>();
	ClassDB::register_class<VoxelGeneratorImage>();
	ClassDB::register_class<VoxelGeneratorNoise2D>();
	ClassDB::register_class<VoxelGeneratorNoise>();
	ClassDB::register_class<VoxelGeneratorGraph>();
	ClassDB::register_class<VoxelGeneratorScript>();

	// Utilities
	ClassDB::register_class<VoxelBoxMover>();
	ClassDB::register_class<VoxelRaycastResult>();
	ClassDB::register_class<VoxelTool>();
	ClassDB::register_class<VoxelToolTerrain>();
	ClassDB::register_class<VoxelBlockSerializer>();
	ClassDB::register_class<VoxelVoxLoader>();

	// Meshers
	ClassDB::register_class<VoxelMesher>();
	ClassDB::register_class<VoxelMesherBlocky>();
	ClassDB::register_class<VoxelMesherTransvoxel>();
	ClassDB::register_class<VoxelMesherDMC>();
	ClassDB::register_class<VoxelMesherCubes>();

	// Reminder: how to create a singleton accessible from scripts:
	// Engine::get_singleton()->add_singleton(Engine::Singleton("SingletonName",singleton_instance));

	PRINT_VERBOSE(String("Size of VoxelBuffer: {0}").format(varray((int)sizeof(VoxelBuffer))));
	PRINT_VERBOSE(String("Size of VoxelBlock: {0}").format(varray((int)sizeof(VoxelBlock))));

#ifdef TOOLS_ENABLED
	EditorPlugins::add_by_type<VoxelGraphEditorPlugin>();
	EditorPlugins::add_by_type<VoxelTerrainEditorPlugin>();
#endif
}

void unregister_voxel_types() {
	// At this point, the GDScript module has nullified GDScriptLanguage::singleton!!
	// That means it's impossible to free scripts still referenced by VoxelServer. And that can happen, because
	// users can write custom generators, which run inside threads, and these threads are hosted in the server...
	// See https://github.com/Zylann/godot_voxel/issues/189

	VoxelStringNames::destroy_singleton();
	VoxelGraphNodeDB::destroy_singleton();
	VoxelServer::destroy_singleton();

	// Do this last as VoxelServer might still be holding some refs to voxel blocks
	unsigned int used_blocks = VoxelMemoryPool::get_singleton()->debug_get_used_blocks();
	if (used_blocks > 0) {
		ERR_PRINT(String("VoxelMemoryPool: "
						 "{0} memory blocks are still used when unregistering the module. Recycling leak?")
						  .format(varray(used_blocks)));
	}
	VoxelMemoryPool::destroy_singleton();
	// TODO No remove?

#ifdef TOOLS_ENABLED
	VoxelDebug::free_resources();

	// TODO Seriously, no remove?
	//EditorPlugins::remove_by_type<VoxelGraphEditorPlugin>();
#endif
}
