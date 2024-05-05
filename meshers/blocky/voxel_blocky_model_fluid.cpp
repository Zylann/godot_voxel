#include "voxel_blocky_model_fluid.h"
// #include "../../util/godot/classes/object.h"
// #include "../../util/string/format.h"
#include "../../util/containers/container_funcs.h"
#include "../../util/godot/core/array.h"
#include "voxel_blocky_library_base.h"
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

namespace {

void bake_fluid_model(
		const VoxelBlockyModelFluid &fluid_model,
		VoxelBlockyModel::BakedData &baked_model,
		StdVector<Ref<VoxelBlockyFluid>> &indexed_fluids,
		StdVector<VoxelBlockyFluid::BakedData> &baked_fluids
) {
	Ref<VoxelBlockyFluid> fluid = fluid_model.get_fluid();

	baked_model.clear();

	if (fluid.is_null()) {
		ZN_PRINT_ERROR("Fluid model without assigned fluid");
		return;
	}

	size_t fluid_index;
	if (!find(indexed_fluids, fluid, fluid_index)) {
		fluid_index = indexed_fluids.size();

		if (fluid_index >= VoxelBlockyLibraryBase::MAX_FLUIDS) {
			ZN_PRINT_ERROR("Reached maximum fluids");
			return;
		}

		indexed_fluids.push_back(fluid);
	}

	baked_model.fluid_index = fluid_index;
	baked_model.empty = false;

	if (fluid_index >= baked_fluids.size()) {
		baked_fluids.resize(fluid_index + 1);
	}

	VoxelBlockyFluid::BakedData &baked_fluid = baked_fluids[fluid_index];

	// TODO Allow more than one model with the same level?
	const unsigned int level = fluid_model.get_level();
	ZN_ASSERT(level >= 0 && level < VoxelBlockyModelFluid::MAX_LEVELS);
	baked_model.fluid_level = level;
	baked_fluid.max_level = math::max(static_cast<uint8_t>(level), baked_fluid.max_level);

	baked_model.transparency_index = fluid_model.get_transparency_index();
	baked_model.culls_neighbors = fluid_model.get_culls_neighbors();
	baked_model.color = fluid_model.get_color();
	baked_model.is_random_tickable = fluid_model.is_random_tickable();
	baked_model.box_collision_mask = fluid_model.get_collision_mask();
	// baked_model.box_collision_aabbs = fluid_model.get_collision_aabbs();

	// This is to be decided dynamically. The top side is always empty.
	baked_model.model.empty_sides_mask = (1 << Cube::SIDE_POSITIVE_Y);
}

} // namespace

void VoxelBlockyModelFluid::bake(blocky::ModelBakingContext &ctx) const {
	bake_fluid_model(*this, ctx.model, ctx.indexed_fluids, ctx.baked_fluids);
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
