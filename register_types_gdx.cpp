#include <godot_cpp/core/class_db.hpp>

#include "constants/voxel_string_names.h"
#include "edition/voxel_tool.h"
#include "edition/voxel_tool_buffer.h"
#include "engine/voxel_engine.h"
#include "engine/voxel_engine_gd.h"
#include "generators/graph/voxel_generator_graph.h"
#include "generators/graph/voxel_graph_node_db.h"
#include "generators/simple/voxel_generator_flat.h"
#include "generators/simple/voxel_generator_heightmap.h"
#include "generators/simple/voxel_generator_image.h"
#include "generators/simple/voxel_generator_noise.h"
#include "generators/simple/voxel_generator_noise_2d.h"
#include "generators/simple/voxel_generator_waves.h"
#include "generators/voxel_generator.h"
#include "generators/voxel_generator_script.h"
#include "meshers/blocky/voxel_blocky_library.h"
#include "meshers/blocky/voxel_blocky_model.h"
#include "meshers/blocky/voxel_mesher_blocky.h"
#include "meshers/cubes/voxel_mesher_cubes.h"
#include "meshers/dmc/voxel_mesher_dmc.h"
#include "meshers/transvoxel/voxel_mesher_transvoxel.h"
#include "meshers/voxel_mesher.h"
#include "storage/modifiers_gd.h"
#include "storage/voxel_buffer_gd.h"
#include "storage/voxel_memory_pool.h"
#include "storage/voxel_metadata_variant.h"
#include "streams/region/voxel_stream_region_files.h"
#include "streams/sqlite/voxel_stream_sqlite.h"
#include "streams/vox/vox_loader.h"
#include "streams/voxel_block_serializer.h"
#include "streams/voxel_block_serializer_gd.h"
#include "streams/voxel_stream_script.h"
#include "terrain/fixed_lod/voxel_terrain.h"
#include "terrain/voxel_data_block_enter_info.h"
#include "terrain/voxel_save_completion_tracker.h"
#include "util/godot/engine.h"
#include "util/godot/rendering_server.h"
#include "util/noise/fast_noise_lite/fast_noise_lite.h"
#include "util/noise/fast_noise_lite/fast_noise_lite_gradient.h"
#include "util/thread/godot_thread_helper.h"

using namespace godot;
using namespace zylann;
using namespace zylann::voxel;

// TODO GDX: Share code between this and the module version

void initialize_extension_test_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		VoxelMemoryPool::create_singleton();
		VoxelStringNames::create_singleton();
		VoxelGraphNodeDB::create_singleton();

		unsigned int main_thread_budget_usec;
		const VoxelEngine::ThreadsConfig threads_config =
				gd::VoxelEngine::get_config_from_godot(main_thread_budget_usec);
		VoxelEngine::create_singleton(threads_config);
		VoxelEngine::get_singleton().set_main_thread_time_budget_usec(main_thread_budget_usec);
		// TODO Pick this from the current renderer + user option (at time of writing, Godot 4 has only one renderer and
		// has not figured out how such option would be exposed).
		// Could use `can_create_resources_async` but this is internal.
		// AFAIK `is_low_end` will be `true` only for OpenGL backends, which are the only ones not supporting async
		// resource creation.
		// TODO GDX: RenderingServer::is_low_end() is not exposed, can't tell if we can generate graphics resources in
		// different threads
		// VoxelEngine::get_singleton().set_threaded_graphics_resource_building_enabled(
		// 		RenderingServer::get_singleton()->is_low_end() == false);

		add_godot_singleton("VoxelEngine", gd::VoxelEngine::get_singleton());

		VoxelMetadataFactory::get_singleton().add_constructor_by_type<gd::VoxelMetadataVariant>(
				gd::METADATA_TYPE_VARIANT);

		VoxelMesherTransvoxel::load_static_resources();

		ClassDB::register_class<VoxelTool>(); // TODO GDX: This class needs to be abstract
		ClassDB::register_class<VoxelToolBuffer>(); // TODO GDX: This class needs to be non-instantiable by scripts

		ClassDB::register_class<gd::VoxelBuffer>();
		ClassDB::register_class<gd::VoxelBlockSerializer>();
		ClassDB::register_class<VoxelVoxLoader>();
		ClassDB::register_class<VoxelSaveCompletionTracker>();
		ClassDB::register_class<VoxelDataBlockEnterInfo>();

		ClassDB::register_class<VoxelGenerator>();
		ClassDB::register_class<VoxelGeneratorScript>();
		ClassDB::register_class<VoxelGeneratorFlat>();
		ClassDB::register_class<VoxelGeneratorHeightmap>(); // TODO GDX: This class needs to be abstract
		ClassDB::register_class<VoxelGeneratorWaves>();
		ClassDB::register_class<VoxelGeneratorImage>();
		ClassDB::register_class<VoxelGeneratorNoise2D>();
		ClassDB::register_class<VoxelGeneratorNoise>();
		ClassDB::register_class<VoxelGeneratorGraph>();

		ClassDB::register_class<VoxelStream>();
		ClassDB::register_class<VoxelStreamScript>();
		ClassDB::register_class<VoxelStreamRegionFiles>();
		ClassDB::register_class<VoxelStreamSQLite>();

		ClassDB::register_class<VoxelBlockyLibrary>();
		ClassDB::register_class<VoxelBlockyModel>();
		ClassDB::register_class<VoxelColorPalette>();

		ClassDB::register_class<VoxelMesher>(); // TODO GDX: This class needs to be abstract
		ClassDB::register_class<VoxelMesherTransvoxel>();
		ClassDB::register_class<VoxelMesherBlocky>();
		ClassDB::register_class<VoxelMesherCubes>();
		ClassDB::register_class<VoxelMesherDMC>();

		ClassDB::register_class<VoxelNode>(); // TODO GDX: This class needs to be abstract
		ClassDB::register_class<VoxelTerrain>();

		ClassDB::register_class<gd::VoxelModifier>(); // TODO GDX: This class needs to be abstract
		ClassDB::register_class<gd::VoxelModifierSphere>();
		ClassDB::register_class<gd::VoxelModifierMesh>();

		ClassDB::register_class<ZN_FastNoiseLite>();
		ClassDB::register_class<ZN_FastNoiseLiteGradient>();

		// TODO GDX: I don't want to expose this one but there is no way not to expose it
		ClassDB::register_class<ZN_GodotThreadHelper>();
	}
}

void uninitialize_extension_test_module(godot::ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		remove_godot_singleton("VoxelEngine");

		VoxelMesherTransvoxel::free_static_resources();
		VoxelStringNames::destroy_singleton();
		VoxelGraphNodeDB::destroy_singleton();
		gd::VoxelEngine::destroy_singleton();
		VoxelEngine::destroy_singleton();

		// Do this last as VoxelEngine might still be holding some refs to voxel blocks
		VoxelMemoryPool::destroy_singleton();
	}
}

extern "C" {
// Library entry point
GDNativeBool GDN_EXPORT voxel_library_init(const GDNativeInterface *p_interface,
		const GDNativeExtensionClassLibraryPtr p_library, GDNativeInitialization *r_initialization) {
	godot::GDExtensionBinding::InitObject init_obj(p_interface, p_library, r_initialization);

	init_obj.register_initializer(initialize_extension_test_module);
	init_obj.register_terminator(uninitialize_extension_test_module);
	init_obj.set_minimum_library_initialization_level(godot::MODULE_INITIALIZATION_LEVEL_SCENE);

	return init_obj.init();
}
}
