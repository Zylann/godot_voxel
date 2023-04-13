#ifdef ZN_GODOT
// Module specific
#include "register_types.h"
#endif

#include "constants/voxel_string_names.h"
#include "edition/voxel_mesh_sdf_gd.h"
#include "edition/voxel_tool.h"
#include "edition/voxel_tool_buffer.h"
#include "edition/voxel_tool_lod_terrain.h"
#include "edition/voxel_tool_terrain.h"
#include "engine/voxel_engine_gd.h"
#include "generators/graph/node_type_db.h"
#include "generators/graph/voxel_generator_graph.h"
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
#include "modifiers/godot/voxel_modifier_gd.h"
#include "modifiers/godot/voxel_modifier_mesh_gd.h"
#include "modifiers/godot/voxel_modifier_sphere_gd.h"
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
#include "terrain/fixed_lod/voxel_terrain_multiplayer_synchronizer.h"
#include "terrain/instancing/voxel_instance_component.h"
#include "terrain/instancing/voxel_instance_library_scene_item.h"
#include "terrain/instancing/voxel_instancer.h"
#include "terrain/variable_lod/voxel_lod_terrain.h"
#include "terrain/voxel_mesh_block.h"
#include "terrain/voxel_save_completion_tracker.h"
#include "terrain/voxel_viewer.h"
#include "util/macros.h"
#include "util/noise/fast_noise_lite/fast_noise_lite.h"
#include "util/noise/fast_noise_lite/fast_noise_lite_gradient.h"
#include "util/string_funcs.h"
#include "util/tasks/async_dependency_tracker.h"
#include "util/tasks/godot/threaded_task_gd.h"

#ifdef ZN_GODOT_EXTENSION
#include "util/thread/godot_thread_helper.h"
#endif

#ifdef VOXEL_ENABLE_FAST_NOISE_2
#include "util/noise/fast_noise_2.h"
#endif

#include "util/godot/classes/engine.h"
#include "util/godot/classes/project_settings.h"
#include "util/godot/core/class_db.h"
// Just for size reminders
#include "util/godot/classes/control.h"
#include "util/godot/classes/mesh_instance_3d.h"
#include "util/godot/classes/sprite_2d.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef TOOLS_ENABLED

#ifdef ZN_GODOT
#include "editor/editor_plugin.h"
#endif
#include "editor/fast_noise_lite/fast_noise_lite_editor_plugin.h"
#include "editor/graph/voxel_graph_editor_node_preview.h"
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

#ifdef ZN_GODOT_EXTENSION
#include "editor/about_window.h"
#include "editor/fast_noise_lite/fast_noise_lite_editor_inspector_plugin.h"
#include "editor/fast_noise_lite/fast_noise_lite_viewer.h"
#include "editor/graph/editor_property_text_change_on_submit.h"
#include "editor/graph/voxel_graph_editor.h"
#include "editor/graph/voxel_graph_editor_inspector_plugin.h"
#include "editor/graph/voxel_graph_editor_io_dialog.h"
#include "editor/graph/voxel_graph_editor_node.h"
#include "editor/graph/voxel_graph_editor_node_preview.h"
#include "editor/graph/voxel_graph_editor_shader_dialog.h"
#include "editor/graph/voxel_graph_editor_window.h"
#include "editor/graph/voxel_graph_function_inspector_plugin.h"
#include "editor/graph/voxel_graph_node_inspector_wrapper.h"
#include "editor/graph/voxel_range_analysis_dialog.h"
#include "editor/instance_library/voxel_instance_library_inspector_plugin.h"
#include "editor/instance_library/voxel_instance_library_multimesh_item_inspector_plugin.h"
#include "editor/instancer/voxel_instancer_stat_view.h"
#include "editor/mesh_sdf/voxel_mesh_sdf_viewer.h"
#include "editor/terrain/voxel_terrain_editor_task_indicator.h"
#endif

#endif // TOOLS_ENABLED

#ifdef VOXEL_RUN_TESTS
#include "tests/tests.h"
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// This is used to have an idea of the memory footprint of various objects as Godot and Voxel development progresses.
void print_size_reminders() {
	using namespace zylann;
	using namespace voxel;

	// Note, this only logs the base size each of these classes. They can often have a bigger memory
	// footprint due to dynamically-allocated members (arrays, dictionaries, RIDs referring to even more data in
	// RenderingServer...)

	ZN_PRINT_VERBOSE(format("Size of Variant: {}", sizeof(Variant)));
	ZN_PRINT_VERBOSE(format("Size of Object: {}", sizeof(Object)));
	ZN_PRINT_VERBOSE(format("Size of RefCounted: {}", sizeof(RefCounted)));
	ZN_PRINT_VERBOSE(format("Size of Node: {}", sizeof(Node)));
	ZN_PRINT_VERBOSE(format("Size of Node3D: {}", sizeof(Node3D)));
	ZN_PRINT_VERBOSE(format("Size of MeshInstance3D: {}", sizeof(MeshInstance3D)));
	ZN_PRINT_VERBOSE(format("Size of GeometryInstance3D: {}", sizeof(MeshInstance3D)));
	ZN_PRINT_VERBOSE(format("Size of Resource: {}", sizeof(Resource)));
	ZN_PRINT_VERBOSE(format("Size of Mesh: {}", sizeof(Mesh)));
	ZN_PRINT_VERBOSE(format("Size of ArrayMesh: {}", sizeof(ArrayMesh)));

	ZN_PRINT_VERBOSE(format("Size of CanvasItem: {}", sizeof(CanvasItem)));
	ZN_PRINT_VERBOSE(format("Size of Node2D: {}", sizeof(Node2D)));
	ZN_PRINT_VERBOSE(format("Size of Sprite2D: {}", sizeof(Sprite2D)));
	ZN_PRINT_VERBOSE(format("Size of Control: {}", sizeof(Control)));

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
	ZN_PRINT_VERBOSE(format("Size of AsyncDependencyTracker: {}", sizeof(AsyncDependencyTracker)));
}

void initialize_voxel_module(ModuleInitializationLevel p_level) {
	using namespace zylann;
	using namespace voxel;

	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		VoxelMemoryPool::create_singleton();
		VoxelStringNames::create_singleton();
		pg::NodeTypeDB::create_singleton();

		unsigned int main_thread_budget_usec;
		const VoxelEngine::ThreadsConfig threads_config =
				gd::VoxelEngine::get_config_from_godot(main_thread_budget_usec);
		VoxelEngine::create_singleton(threads_config);
		VoxelEngine::get_singleton().set_main_thread_time_budget_usec(main_thread_budget_usec);
#if defined(ZN_GODOT)
		// TODO Enhancement: threaded graphics resource building should be initialized better.
		// Pick this from the current renderer + user option (at time of writing, Godot 4 has only one
		// renderer and has not figured out how such option would be exposed). Could use `can_create_resources_async`
		// but this is internal. AFAIK `is_low_end` will be `true` only for OpenGL backends, which are the only ones not
		// supporting async resource creation.
		VoxelEngine::get_singleton().set_threaded_graphics_resource_building_enabled(
				RenderingServer::get_singleton()->is_low_end() == false);
#else
		// TODO GDX: RenderingServer::is_low_end() is not exposed, can't tell if we can generate graphics resources in
		// different threads
#endif

		gd::VoxelEngine::create_singleton();
		add_godot_singleton("VoxelEngine", gd::VoxelEngine::get_singleton());

		VoxelMetadataFactory::get_singleton().add_constructor_by_type<gd::VoxelMetadataVariant>(
				gd::METADATA_TYPE_VARIANT);

		VoxelMesherTransvoxel::load_static_resources();

		// TODO Enhancement: can I prevent users from instancing `VoxelEngine`?
		// This class is used as a singleton so it's not really abstract.
		// Should I use `register_abstract_class` anyways?
		ClassDB::register_class<gd::VoxelEngine>();

		// Misc
		ClassDB::register_class<VoxelBlockyModel>();
		ClassDB::register_class<VoxelBlockyLibrary>();
		ClassDB::register_class<VoxelColorPalette>();
		ClassDB::register_class<VoxelInstanceLibrary>();
		register_abstract_class<VoxelInstanceLibraryItem>();
		ClassDB::register_class<VoxelInstanceLibraryMultiMeshItem>();
		ClassDB::register_class<VoxelInstanceLibrarySceneItem>();
		ClassDB::register_class<VoxelDataBlockEnterInfo>();
		ClassDB::register_class<VoxelSaveCompletionTracker>();
		ClassDB::register_class<pg::VoxelGraphFunction>();

		// Storage
		ClassDB::register_class<gd::VoxelBuffer>();

		// Nodes
		register_abstract_class<VoxelNode>();
		ClassDB::register_class<VoxelTerrain>();
		ClassDB::register_class<VoxelLodTerrain>();
		ClassDB::register_class<VoxelViewer>();
		ClassDB::register_class<VoxelInstanceGenerator>();
		ClassDB::register_class<VoxelInstancer>();
		ClassDB::register_class<VoxelInstanceComponent>();
		register_abstract_class<gd::VoxelModifier>();
		ClassDB::register_class<gd::VoxelModifierSphere>();
		ClassDB::register_class<gd::VoxelModifierMesh>();

		// Streams
		register_abstract_class<VoxelStream>();
		ClassDB::register_class<VoxelStreamRegionFiles>();
		ClassDB::register_class<VoxelStreamScript>();
		ClassDB::register_class<VoxelStreamSQLite>();

		// Generators
		register_abstract_class<VoxelGenerator>();
		ClassDB::register_class<VoxelGeneratorFlat>();
		ClassDB::register_class<VoxelGeneratorWaves>();
		register_abstract_class<VoxelGeneratorHeightmap>();
		ClassDB::register_class<VoxelGeneratorImage>();
		ClassDB::register_class<VoxelGeneratorNoise2D>();
		ClassDB::register_class<VoxelGeneratorNoise>();
		ClassDB::register_class<VoxelGeneratorGraph>();
		ClassDB::register_class<VoxelGeneratorScript>();

		// Utilities
		ClassDB::register_class<VoxelBoxMover>();
		ClassDB::register_class<VoxelRaycastResult>();
		register_abstract_class<VoxelTool>();
		register_abstract_class<VoxelToolTerrain>();
		register_abstract_class<VoxelToolLodTerrain>();
		// I had to bind this one despite it being useless as-is because otherwise Godot lazily initializes its class.
		// And this can happen in a thread, causing crashes due to the concurrent access
		register_abstract_class<VoxelToolBuffer>();
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
		ClassDB::register_class<VoxelTerrainMultiplayerSynchronizer>();

		// Meshers
		register_abstract_class<VoxelMesher>();
		ClassDB::register_class<VoxelMesherBlocky>();
		ClassDB::register_class<VoxelMesherTransvoxel>();
		ClassDB::register_class<VoxelMesherDMC>();
		ClassDB::register_class<VoxelMesherCubes>();

#ifdef ZN_GODOT_EXTENSION
		// TODO GDX: I don't want to expose this one but there is no way not to expose it
		ClassDB::register_class<ZN_GodotThreadHelper>();
#endif

		print_size_reminders();

#ifdef ZN_GODOT
		if (RenderingDevice::get_singleton() != nullptr) {
			ZN_PRINT_VERBOSE(format("TextureArray max layers: {}",
					RenderingDevice::get_singleton()->limit_get(RenderingDevice::LIMIT_MAX_TEXTURE_ARRAY_LAYERS)));
		}
#else
		// TODO GDX: Not possible to access the default `RenderingDevice` to query its limits
#endif

#ifdef ZN_GODOT
		// Compatibility with older version
		ClassDB::add_compatibility_class("VoxelLibrary", "VoxelBlockyLibrary");
		ClassDB::add_compatibility_class("Voxel", "VoxelBlockyModel");
		ClassDB::add_compatibility_class("VoxelInstanceLibraryItem", "VoxelInstanceLibraryMultiMeshItem");
		// Not possible to add a compat class for this one because the new name is indistinguishable from an old one.
		// However this is an abstract class so it should not be found in resources hopefully
		// ClassDB::add_compatibility_class("VoxelInstanceLibraryItemBase", "VoxelInstanceLibraryItem");
#endif

#ifdef VOXEL_RUN_TESTS
		zylann::voxel::tests::run_voxel_tests();
#endif
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		VoxelGraphEditorNodePreview::load_resources();

#if defined(ZN_GODOT)
		EditorPlugins::add_by_type<VoxelGraphEditorPlugin>();
		EditorPlugins::add_by_type<VoxelTerrainEditorPlugin>();
		EditorPlugins::add_by_type<VoxelInstanceLibraryEditorPlugin>();
		EditorPlugins::add_by_type<VoxelInstanceLibraryMultiMeshItemEditorPlugin>();
		EditorPlugins::add_by_type<ZN_FastNoiseLiteEditorPlugin>();
		EditorPlugins::add_by_type<magica::VoxelVoxEditorPlugin>();
		EditorPlugins::add_by_type<VoxelInstancerEditorPlugin>();
		EditorPlugins::add_by_type<VoxelMeshSDFEditorPlugin>();
#ifdef VOXEL_ENABLE_FAST_NOISE_2
		EditorPlugins::add_by_type<FastNoise2EditorPlugin>();
#endif

#elif defined(ZN_GODOT_EXTENSION)
		// TODO GDX: Can't add plugins.
		// See https://github.com/godotengine/godot-cpp/issues/640
		// and https://github.com/godotengine/godot/pull/65592

		// TODO GDX: I don't want to expose any of the following classes, but it looks like there is no way to make them
		// functional as extensions BUT not expose them

		ClassDB::register_class<VoxelAboutWindow>();
		ClassDB::register_class<VoxelTerrainEditorInspectorPlugin>();
		ClassDB::register_class<VoxelTerrainEditorPlugin>();
		ClassDB::register_class<VoxelTerrainEditorTaskIndicator>();

		ClassDB::register_class<ZN_FastNoiseLiteEditorPlugin>();
		ClassDB::register_class<ZN_FastNoiseLiteEditorInspectorPlugin>();
		ClassDB::register_class<ZN_FastNoiseLiteViewer>();

		ClassDB::register_class<ZN_EditorPlugin>();
		ClassDB::register_class<ZN_EditorImportPlugin>();
		ClassDB::register_class<ZN_EditorInspectorPlugin>();
		ClassDB::register_class<ZN_EditorProperty>();

		ClassDB::register_class<magica::VoxelVoxEditorPlugin>();
		ClassDB::register_class<magica::VoxelVoxMeshImporter>();
		ClassDB::register_class<magica::VoxelVoxSceneImporter>();

		ClassDB::register_class<VoxelInstancerEditorPlugin>();
		ClassDB::register_class<VoxelInstancerStatView>();

		ClassDB::register_class<VoxelInstanceLibraryEditorPlugin>();
		ClassDB::register_class<VoxelInstanceLibraryInspectorPlugin>();
		ClassDB::register_class<VoxelInstanceLibraryMultiMeshItemEditorPlugin>();
		ClassDB::register_class<VoxelInstanceLibraryMultiMeshItemInspectorPlugin>();

		ClassDB::register_class<VoxelMeshSDFViewer>();
		ClassDB::register_class<VoxelMeshSDFEditorPlugin>();
		ClassDB::register_class<VoxelMeshSDFInspectorPlugin>();

		ClassDB::register_class<ZN_EditorPropertyTextChangeOnSubmit>();
		ClassDB::register_class<VoxelGraphEditorInspectorPlugin>();
		ClassDB::register_class<VoxelGraphFunctionInspectorPlugin>();
		ClassDB::register_class<VoxelGraphEditorNodePreview>();
		ClassDB::register_class<VoxelGraphEditorNode>();
		ClassDB::register_class<VoxelGraphEditor>();
		ClassDB::register_class<VoxelGraphEditorPlugin>();
		ClassDB::register_class<VoxelGraphEditorShaderDialog>();
		ClassDB::register_class<VoxelGraphEditorIODialog>();
		ClassDB::register_class<VoxelGraphNodeInspectorWrapper>();
		ClassDB::register_class<VoxelRangeAnalysisDialog>();
#endif
	}
#endif // TOOLS_ENABLED
}

void uninitialize_voxel_module(ModuleInitializationLevel p_level) {
	using namespace zylann;
	using namespace voxel;

	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		remove_godot_singleton("VoxelEngine");

		// At this point, the GDScript module has nullified GDScriptLanguage::singleton!!
		// That means it's impossible to free scripts still referenced by VoxelEngine. And that can happen, because
		// users can write custom generators, which run inside threads, and these threads are hosted in the server...
		// See https://github.com/Zylann/godot_voxel/issues/189

		VoxelMesherTransvoxel::free_static_resources();
		VoxelStringNames::destroy_singleton();
		pg::NodeTypeDB::destroy_singleton();
		gd::VoxelEngine::destroy_singleton();
		VoxelEngine::destroy_singleton();

		// Do this last as VoxelEngine might still be holding some refs to voxel blocks
		VoxelMemoryPool::destroy_singleton();
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		zylann::free_debug_resources();
		VoxelGraphEditorNodePreview::unload_resources();

		// TODO GDX: Can't remove plugins.
	}
#endif // TOOLS_ENABLED
}

#ifdef ZN_GODOT_EXTENSION
extern "C" {
// Library entry point
GDExtensionBool GDE_EXPORT voxel_library_init(const GDExtensionInterface *p_interface,
		const GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization) {
	godot::GDExtensionBinding::InitObject init_obj(p_interface, p_library, r_initialization);

	init_obj.register_initializer(initialize_voxel_module);
	init_obj.register_terminator(uninitialize_voxel_module);
	init_obj.set_minimum_library_initialization_level(godot::MODULE_INITIALIZATION_LEVEL_SCENE);

	return init_obj.init();
}
}
#endif
