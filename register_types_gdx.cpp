#include <godot_cpp/core/class_db.hpp>

#include "constants/voxel_string_names.h"
#include "edition/voxel_tool.h"
#include "edition/voxel_tool_buffer.h"
#include "generators/simple/voxel_generator_flat.h"
#include "generators/simple/voxel_generator_heightmap.h"
#include "generators/simple/voxel_generator_image.h"
#include "generators/simple/voxel_generator_noise.h"
#include "generators/simple/voxel_generator_noise_2d.h"
#include "generators/simple/voxel_generator_waves.h"
#include "generators/voxel_generator.h"
#include "generators/voxel_generator_script.h"
#include "storage/voxel_buffer_gd.h"
#include "storage/voxel_memory_pool.h"
#include "storage/voxel_metadata_variant.h"
#include "util/thread/godot_thread_helper.h"

using namespace godot;
using namespace zylann;
using namespace zylann::voxel;

// TODO GDX: Share code between this and the module version

void initialize_extension_test_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		VoxelMemoryPool::create_singleton();
		VoxelStringNames::create_singleton();

		VoxelMetadataFactory::get_singleton().add_constructor_by_type<gd::VoxelMetadataVariant>(
				gd::METADATA_TYPE_VARIANT);

		ClassDB::register_class<VoxelTool>(); // TODO GDX: This class needs to be abstract
		ClassDB::register_class<VoxelToolBuffer>(); // TODO GDX: This class needs to be non-instantiable by scripts

		ClassDB::register_class<gd::VoxelBuffer>();

		ClassDB::register_class<VoxelGenerator>();
		ClassDB::register_class<VoxelGeneratorScript>();
		ClassDB::register_class<VoxelGeneratorFlat>();
		ClassDB::register_class<VoxelGeneratorHeightmap>(); // TODO GDX: This class needs to be abstract
		ClassDB::register_class<VoxelGeneratorWaves>();
		ClassDB::register_class<VoxelGeneratorImage>();
		ClassDB::register_class<VoxelGeneratorNoise2D>();
		ClassDB::register_class<VoxelGeneratorNoise>();

		// TODO GDX: I don't want to expose this one but there is no way not to expose it
		ClassDB::register_class<ZN_GodotThreadHelper>();
	}
}

void uninitialize_extension_test_module(godot::ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		//VoxelMesherTransvoxel::free_static_resources();
		VoxelStringNames::destroy_singleton();
		//VoxelGraphNodeDB::destroy_singleton();
		//gd::VoxelEngine::destroy_singleton();
		//VoxelEngine::destroy_singleton();

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
