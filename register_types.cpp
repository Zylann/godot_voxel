#include "register_types.h"
#include "constants/voxel_string_names.h"
#include "edition/voxel_mesh_sdf_gd.h"
#include "edition/voxel_tool.h"
#include "edition/voxel_tool_buffer.h"
#include "edition/voxel_tool_lod_terrain.h"
#include "edition/voxel_tool_terrain.h"
#include "engine/voxel_engine_gd.h"
#include "generators/graph/voxel_generator_graph.h"
#include "generators/graph/voxel_graph_node_db.h"
#include "generators/simple/voxel_generator_flat.h"
#include "generators/simple/voxel_generator_heightmap.h"
#include "generators/simple/voxel_generator_image.h"
#include "generators/simple/voxel_generator_noise.h"
#include "generators/simple/voxel_generator_noise_2d.h"
#include "generators/simple/voxel_generator_waves.h"
#include "generators/voxel_generator_script.h"
#include "meshers/blocky/voxel_blocky_library.h"
#include "meshers/blocky/voxel_mesher_blocky.h"
#include "meshers/cubes/voxel_mesher_cubes.h"
#include "meshers/dmc/voxel_mesher_dmc.h"
#include "meshers/transvoxel/voxel_mesher_transvoxel.h"
#include "storage/modifiers_gd.h"
#include "storage/voxel_buffer_gd.h"
#include "storage/voxel_memory_pool.h"
#include "storage/voxel_metadata_variant.h"
#include "streams/region/voxel_stream_region_files.h"
#include "streams/sqlite/voxel_stream_sqlite.h"
#include "streams/vox/vox_loader.h"
#include "streams/voxel_block_serializer_gd.h"
#include "streams/voxel_stream_script.h"
#include "terrain/fixed_lod/voxel_box_mover.h"
#include "terrain/fixed_lod/voxel_terrain.h"
#include "terrain/instancing/voxel_instance_component.h"
#include "terrain/instancing/voxel_instance_library_scene_item.h"
#include "terrain/instancing/voxel_instancer.h"
#include "terrain/variable_lod/voxel_lod_terrain.h"
#include "terrain/voxel_mesh_block.h"
#include "terrain/voxel_viewer.h"
#include "util/macros.h"
#include "util/noise/fast_noise_lite/fast_noise_lite.h"
#include "util/noise/fast_noise_lite/fast_noise_lite_gradient.h"
#include "util/string_funcs.h"
#include "util/tasks/godot/threaded_task_gd.h"

#ifdef VOXEL_ENABLE_FAST_NOISE_2
#include "util/noise/fast_noise_2.h"
#endif

#include <core/config/engine.h>
#include <core/config/project_settings.h>

#ifdef TOOLS_ENABLED
#include "editor/editor_plugin.h"
#include "editor/fast_noise_lite/fast_noise_lite_editor_plugin.h"
#include "editor/graph/voxel_graph_editor_plugin.h"
#include "editor/instance_library/voxel_instance_library_editor_plugin.h"
#include "editor/instance_library/voxel_instance_library_multimesh_item_editor_plugin.h"
#include "editor/instancer/voxel_instancer_editor_plugin.h"
#include "editor/mesh_sdf/voxel_mesh_sdf_editor_plugin.h"
#include "editor/terrain/voxel_terrain_editor_plugin.h"
#include "editor/vox/vox_editor_plugin.h"
#include "editor/voxel_debug.h"
#ifdef VOXEL_ENABLE_FAST_NOISE_2
#include "editor/fast_noise_2/fast_noise_2_editor_plugin.h"
#endif
#endif // TOOLS_ENABLED

#ifdef VOXEL_RUN_TESTS
#include "tests/tests.h"
#endif

namespace zylann::voxel {

static VoxelEngine::ThreadsConfig get_config_from_godot(unsigned int &out_main_thread_time_budget_usec) {
	CRASH_COND(ProjectSettings::get_singleton() == nullptr);

	VoxelEngine::ThreadsConfig config;

	// Compute thread count for general pool.
	// Note that the I/O thread counts as one used thread and will always be present.

	// "RST" means changing the property requires an editor restart (or game restart)
	GLOBAL_DEF_RST("voxel/threads/count/minimum", 1);
	ProjectSettings::get_singleton()->set_custom_property_info("voxel/threads/count/minimum",
			PropertyInfo(Variant::INT, "voxel/threads/count/minimum", PROPERTY_HINT_RANGE, "1,64"));

	GLOBAL_DEF_RST("voxel/threads/count/margin_below_max", 1);
	ProjectSettings::get_singleton()->set_custom_property_info("voxel/threads/count/margin_below_max",
			PropertyInfo(Variant::INT, "voxel/threads/count/margin_below_max", PROPERTY_HINT_RANGE, "1,64"));

	GLOBAL_DEF_RST("voxel/threads/count/ratio_over_max", 0.5f);
	ProjectSettings::get_singleton()->set_custom_property_info("voxel/threads/count/ratio_over_max",
			PropertyInfo(Variant::FLOAT, "voxel/threads/count/ratio_over_max", PROPERTY_HINT_RANGE, "0,1,0.1"));

	GLOBAL_DEF_RST("voxel/threads/main/time_budget_ms", 8);
	ProjectSettings::get_singleton()->set_custom_property_info("voxel/threads/main/time_budget_ms",
			PropertyInfo(Variant::INT, "voxel/threads/main/time_budget_ms", PROPERTY_HINT_RANGE, "0,1000"));

	out_main_thread_time_budget_usec =
			1000 * int(ProjectSettings::get_singleton()->get("voxel/threads/main/time_budget_ms"));

	config.thread_count_minimum =
			math::max(1, int(ProjectSettings::get_singleton()->get("voxel/threads/count/minimum")));

	// How many threads below available count on the CPU should we set as limit
	config.thread_count_margin_below_max =
			math::max(1, int(ProjectSettings::get_singleton()->get("voxel/threads/count/margin_below_max")));

	// Portion of available CPU threads to attempt using
	config.thread_count_ratio_over_max = zylann::math::clamp(
			float(ProjectSettings::get_singleton()->get("voxel/threads/count/ratio_over_max")), 0.f, 1.f);

	return config;
}

} // namespace zylann::voxel

void initialize_voxel_module(ModuleInitializationLevel p_level) {
	using namespace zylann;
	using namespace voxel;

	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		VoxelMemoryPool::create_singleton();
		VoxelStringNames::create_singleton();
		VoxelGraphNodeDB::create_singleton();

		unsigned int main_thread_budget_usec;
		const VoxelEngine::ThreadsConfig threads_config = get_config_from_godot(main_thread_budget_usec);
		VoxelEngine::create_singleton(threads_config);
		VoxelEngine::get_singleton().set_main_thread_time_budget_usec(main_thread_budget_usec);
		// TODO Pick this from the current renderer + user option (at time of writing, Godot 4 has only one renderer and
		// has not figured out how such option would be exposed).
		// Could use `can_create_resources_async` but this is internal.
		// AFAIK `is_low_end` will be `true` only for OpenGL backends, which are the only ones not supporting async
		// resource creation.
		VoxelEngine::get_singleton().set_threaded_graphics_resource_building_enabled(
				RenderingServer::get_singleton()->is_low_end() == false);

		gd::VoxelEngine::create_singleton();
		Engine::get_singleton()->add_singleton(Engine::Singleton("VoxelEngine", gd::VoxelEngine::get_singleton()));

		VoxelMetadataFactory::get_singleton().add_constructor_by_type<gd::VoxelMetadataVariant>(
				gd::METADATA_TYPE_VARIANT);

		VoxelMesherTransvoxel::load_static_resources();

		// TODO Can I prevent users from instancing it? is "register_virtual_class" correct for a class that's not
		// abstract?
		ClassDB::register_class<gd::VoxelEngine>();

		// Misc
		ClassDB::register_class<VoxelBlockyModel>();
		ClassDB::register_class<VoxelBlockyLibrary>();
		ClassDB::register_class<VoxelColorPalette>();
		ClassDB::register_class<VoxelInstanceLibrary>();
		ClassDB::register_abstract_class<VoxelInstanceLibraryItem>();
		ClassDB::register_class<VoxelInstanceLibraryMultiMeshItem>();
		ClassDB::register_class<VoxelInstanceLibrarySceneItem>();
		ClassDB::register_class<VoxelDataBlockEnterInfo>();

		// Storage
		ClassDB::register_class<gd::VoxelBuffer>();

		// Nodes
		ClassDB::register_abstract_class<VoxelNode>();
		ClassDB::register_class<VoxelTerrain>();
		ClassDB::register_class<VoxelLodTerrain>();
		ClassDB::register_class<VoxelViewer>();
		ClassDB::register_class<VoxelInstanceGenerator>();
		ClassDB::register_class<VoxelInstancer>();
		ClassDB::register_class<VoxelInstanceComponent>();
		ClassDB::register_abstract_class<gd::VoxelModifier>();
		ClassDB::register_class<gd::VoxelModifierSphere>();
		ClassDB::register_class<gd::VoxelModifierMesh>();

		// Streams
		ClassDB::register_abstract_class<VoxelStream>();
		ClassDB::register_class<VoxelStreamRegionFiles>();
		ClassDB::register_class<VoxelStreamScript>();
		ClassDB::register_class<VoxelStreamSQLite>();

		// Generators
		ClassDB::register_abstract_class<VoxelGenerator>();
		ClassDB::register_class<VoxelGeneratorFlat>();
		ClassDB::register_class<VoxelGeneratorWaves>();
		ClassDB::register_abstract_class<VoxelGeneratorHeightmap>();
		ClassDB::register_class<VoxelGeneratorImage>();
		ClassDB::register_class<VoxelGeneratorNoise2D>();
		ClassDB::register_class<VoxelGeneratorNoise>();
		ClassDB::register_class<VoxelGeneratorGraph>();
		ClassDB::register_class<VoxelGeneratorScript>();

		// Utilities
		ClassDB::register_class<VoxelBoxMover>();
		ClassDB::register_class<VoxelRaycastResult>();
		ClassDB::register_abstract_class<VoxelTool>();
		ClassDB::register_abstract_class<VoxelToolTerrain>();
		ClassDB::register_abstract_class<VoxelToolLodTerrain>();
		// I had to bind this one despite it being useless as-is because otherwise Godot lazily initializes its class.
		// And this can happen in a thread, causing crashes due to the concurrent access
		ClassDB::register_abstract_class<VoxelToolBuffer>();
		ClassDB::register_class<gd::VoxelBlockSerializer>();
		ClassDB::register_class<VoxelVoxLoader>();
		ClassDB::register_class<ZN_FastNoiseLite>();
		ClassDB::register_class<ZN_FastNoiseLiteGradient>();
		ClassDB::register_class<ZN_ThreadedTask>();
		// See SCsub
#ifdef VOXEL_ENABLE_FAST_NOISE_2
		ClassDB::register_class<FastNoise2>();
#endif
		ClassDB::register_class<VoxelMeshSDF>();

		// Meshers
		ClassDB::register_abstract_class<VoxelMesher>();
		ClassDB::register_class<VoxelMesherBlocky>();
		ClassDB::register_class<VoxelMesherTransvoxel>();
		ClassDB::register_class<VoxelMesherDMC>();
		ClassDB::register_class<VoxelMesherCubes>();

		// Reminder: how to create a singleton accessible from scripts:
		// Engine::get_singleton()->add_singleton(Engine::Singleton("SingletonName",singleton_instance));

		// Reminders
		ZN_PRINT_VERBOSE(format("Size of Variant: {}", sizeof(Variant)));
		ZN_PRINT_VERBOSE(format("Size of Object: {}", sizeof(Object)));
		ZN_PRINT_VERBOSE(format("Size of RefCounted: {}", sizeof(RefCounted)));
		ZN_PRINT_VERBOSE(format("Size of Node: {}", sizeof(Node)));
		ZN_PRINT_VERBOSE(format("Size of Node3D: {}", sizeof(Node3D)));
		ZN_PRINT_VERBOSE(format("Size of RWLock: {}", sizeof(zylann::RWLock)));
		ZN_PRINT_VERBOSE(format("Size of Mutex: {}", sizeof(zylann::Mutex)));
		ZN_PRINT_VERBOSE(format("Size of BinaryMutex: {}", sizeof(zylann::BinaryMutex)));
		ZN_PRINT_VERBOSE(format("Size of gd::VoxelBuffer: {}", sizeof(gd::VoxelBuffer)));
		ZN_PRINT_VERBOSE(format("Size of VoxelBufferInternal: {}", sizeof(VoxelBufferInternal)));
		ZN_PRINT_VERBOSE(format("Size of VoxelMeshBlock: {}", sizeof(VoxelMeshBlock)));
		ZN_PRINT_VERBOSE(format("Size of VoxelTerrain: {}", sizeof(VoxelTerrain)));
		ZN_PRINT_VERBOSE(format("Size of VoxelLodTerrain: {}", sizeof(VoxelLodTerrain)));
		ZN_PRINT_VERBOSE(format("Size of VoxelInstancer: {}", sizeof(VoxelInstancer)));
		ZN_PRINT_VERBOSE(format("Size of VoxelDataMap: {}", sizeof(VoxelDataMap)));
		ZN_PRINT_VERBOSE(format("Size of VoxelData: {}", sizeof(VoxelData)));
		ZN_PRINT_VERBOSE(format("Size of VoxelMesher::Output: {}", sizeof(VoxelMesher::Output)));
		ZN_PRINT_VERBOSE(format("Size of VoxelEngine::BlockMeshOutput: {}", sizeof(VoxelEngine::BlockMeshOutput)));
		ZN_PRINT_VERBOSE(format("Size of VoxelModifierStack: {}", sizeof(VoxelModifierStack)));
		if (RenderingDevice::get_singleton() != nullptr) {
			ZN_PRINT_VERBOSE(format("TextureArray max layers: {}",
					RenderingDevice::get_singleton()->limit_get(RenderingDevice::LIMIT_MAX_TEXTURE_ARRAY_LAYERS)));
		}

#ifdef VOXEL_RUN_TESTS
		zylann::voxel::tests::run_voxel_tests();
#endif

		// Compatibility with older version
		ClassDB::add_compatibility_class("VoxelLibrary", "VoxelBlockyLibrary");
		ClassDB::add_compatibility_class("Voxel", "VoxelBlockyModel");
		ClassDB::add_compatibility_class("VoxelInstanceLibraryItem", "VoxelInstanceLibraryMultiMeshItem");
		// Not possible to add a compat class for this one because the new name is indistinguishable from an old one.
		// However this is an abstract class so it should not be found in resources hopefully
		//ClassDB::add_compatibility_class("VoxelInstanceLibraryItemBase", "VoxelInstanceLibraryItem");
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		EditorPlugins::add_by_type<VoxelGraphEditorPlugin>();
		EditorPlugins::add_by_type<VoxelTerrainEditorPlugin>();
		EditorPlugins::add_by_type<VoxelInstanceLibraryEditorPlugin>();
		EditorPlugins::add_by_type<VoxelInstanceLibraryMultiMeshItemEditorPlugin>();
		EditorPlugins::add_by_type<ZN_FastNoiseLiteEditorPlugin>();
		EditorPlugins::add_by_type<magica::VoxEditorPlugin>();
		EditorPlugins::add_by_type<VoxelInstancerEditorPlugin>();
		EditorPlugins::add_by_type<VoxelMeshSDFEditorPlugin>();
#ifdef VOXEL_ENABLE_FAST_NOISE_2
		EditorPlugins::add_by_type<FastNoise2EditorPlugin>();
#endif
	}
#endif // TOOLS_ENABLED
}

void uninitialize_voxel_module(ModuleInitializationLevel p_level) {
	using namespace zylann;
	using namespace voxel;

	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		// At this point, the GDScript module has nullified GDScriptLanguage::singleton!!
		// That means it's impossible to free scripts still referenced by VoxelEngine. And that can happen, because
		// users can write custom generators, which run inside threads, and these threads are hosted in the server...
		// See https://github.com/Zylann/godot_voxel/issues/189

		VoxelMesherTransvoxel::free_static_resources();
		VoxelStringNames::destroy_singleton();
		VoxelGraphNodeDB::destroy_singleton();
		gd::VoxelEngine::destroy_singleton();
		VoxelEngine::destroy_singleton();

		// Do this last as VoxelEngine might still be holding some refs to voxel blocks
		VoxelMemoryPool::destroy_singleton();
		// TODO No remove?
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		zylann::free_debug_resources();

		// TODO Seriously, no remove?
		//EditorPlugins::remove_by_type<VoxelGraphEditorPlugin>();
	}
#endif // TOOLS_ENABLED
}
