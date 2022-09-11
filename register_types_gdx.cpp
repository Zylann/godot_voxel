#include <godot_cpp/core/class_db.hpp>

#include "edition/voxel_tool.h"
#include "edition/voxel_tool_buffer.h"
#include "storage/voxel_buffer_gd.h"
#include "storage/voxel_metadata_variant.h"
#include "util/thread/godot_thread_helper.h"

using namespace godot;
using namespace zylann;
using namespace zylann::voxel;

// TODO Share code between this and the module version

void initialize_extension_test_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	VoxelMetadataFactory::get_singleton().add_constructor_by_type<gd::VoxelMetadataVariant>(gd::METADATA_TYPE_VARIANT);

	ClassDB::register_class<VoxelTool>();
	ClassDB::register_class<VoxelToolBuffer>();
	ClassDB::register_class<gd::VoxelBuffer>();

	// TODO I don't want to expose this one but there is no way not to expose it
	ClassDB::register_class<ZN_GodotThreadHelper>();
}

void uninitialize_extension_test_module(godot::ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
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
