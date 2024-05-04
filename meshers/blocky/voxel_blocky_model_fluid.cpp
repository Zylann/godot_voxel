#include "voxel_blocky_model_fluid.h"
// #include "../../util/godot/classes/object.h"
// #include "../../util/string/format.h"
#include "../../util/godot/core/array.h"
#include "voxel_blocky_model_baking_context.h"

namespace zylann::voxel {

VoxelBlockyModelFluid::VoxelBlockyModelFluid() {}

void VoxelBlockyModelFluid::set_fluid(Ref<VoxelBlockyFluid> fluid) {
	_fluid = fluid;
}

Ref<VoxelBlockyFluid> VoxelBlockyModelFluid::get_fluid() const {
	return _fluid;
}

void VoxelBlockyModelFluid::set_level(int level) {
	_level = math::clamp(level, 0, MAX_LEVELS - 1);
}

int VoxelBlockyModelFluid::get_level() const {
	return _level;
}

void VoxelBlockyModelFluid::bake(blocky::ModelBakingContext &ctx) const {
	// Fluids are baked differently
}

// #ifdef TOOLS_ENABLED

// void VoxelBlockyModelFluid::get_configuration_warnings(PackedStringArray &warnings) {
// 	using namespace zylann::godot;

// 	if (!_fluid.is_valid()) {
// 		warnings.append(String("{0} has no fluid assigned.").format(varray(VoxelBlockyModelFluid::get_class_static())));
// 	}
// }

// #endif

void VoxelBlockyModelFluid::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_fluid", "fluid"), &VoxelBlockyModelFluid::set_fluid);
	ClassDB::bind_method(D_METHOD("get_fluid"), &VoxelBlockyModelFluid::get_fluid);

	ClassDB::bind_method(D_METHOD("set_level", "level"), &VoxelBlockyModelFluid::set_level);
	ClassDB::bind_method(D_METHOD("get_level"), &VoxelBlockyModelFluid::get_level);

	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "fluid", PROPERTY_HINT_RESOURCE_TYPE, VoxelBlockyFluid::get_class_static()),
			"set_fluid",
			"get_fluid"
	);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "level"), "set_level", "get_level");
}

} // namespace zylann::voxel
