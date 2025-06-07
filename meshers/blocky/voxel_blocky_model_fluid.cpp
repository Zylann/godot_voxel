#include "voxel_blocky_model_fluid.h"
// #include "../../util/godot/classes/object.h"
// #include "../../util/string/format.h"
#include "../../constants/voxel_string_names.h"
#include "../../util/containers/container_funcs.h"
#include "../../util/godot/core/array.h"
#include "blocky_fluids.h"
#include "blocky_material_indexer.h"
#include "blocky_model_baking_context.h"
#include "voxel_blocky_library_base.h"

namespace zylann::voxel {

VoxelBlockyModelFluid::VoxelBlockyModelFluid() {}

void VoxelBlockyModelFluid::set_fluid(Ref<VoxelBlockyFluid> fluid) {
	if (fluid == _fluid) {
		return;
	}
	if (_fluid.is_valid()) {
		_fluid->disconnect(
				VoxelStringNames::get_singleton().changed, callable_mp(this, &VoxelBlockyModelFluid::_on_fluid_changed)
		);
	}
	_fluid = fluid;
	if (_fluid.is_valid()) {
		_fluid->connect(
				VoxelStringNames::get_singleton().changed, callable_mp(this, &VoxelBlockyModelFluid::_on_fluid_changed)
		);
	}
	emit_changed();
}

Ref<VoxelBlockyFluid> VoxelBlockyModelFluid::get_fluid() const {
	return _fluid;
}

void VoxelBlockyModelFluid::set_level(const int level) {
	const unsigned int checked_level = static_cast<unsigned int>(math::clamp(level, 0, MAX_LEVELS - 1));
	if (checked_level == _level) {
		return;
	}
	_level = checked_level;
	emit_changed();
}

int VoxelBlockyModelFluid::get_level() const {
	return _level;
}

bool VoxelBlockyModelFluid::is_empty() const {
	return _fluid.is_null();
}

Ref<Mesh> VoxelBlockyModelFluid::get_preview_mesh() const {
	if (_fluid.is_null()) {
		return Ref<Mesh>();
	}

	StdVector<Ref<Material>> materials;
	blocky::MaterialIndexer material_indexer{ materials };

	blocky::BakedLibrary library;
	library.models.resize(2);

	const bool tangents_enabled = false;

	StdVector<Ref<VoxelBlockyFluid>> indexed_fluids;
	blocky::ModelBakingContext model_baking_context{
		library.models[1], tangents_enabled, material_indexer, indexed_fluids, library.fluids
	};
	bake(model_baking_context);

	library.fluids.resize(1);
	_fluid->bake(library.fluids[0], material_indexer);

	Span<const blocky::BakedModel::Surface> model_surfaces;
	const FixedArray<FixedArray<blocky::BakedModel::SideSurface, blocky::MAX_SURFACES>, Cube::SIDE_COUNT>
			*model_sides_surfaces = nullptr;

	blocky::generate_preview_fluid_model(library.models[1], 1, library, model_surfaces, model_sides_surfaces);
	ZN_ASSERT_RETURN_V(model_sides_surfaces != nullptr, Ref<Mesh>());

	Ref<Mesh> mesh =
			make_mesh_from_baked_data(model_surfaces, to_span(*model_sides_surfaces), Color(1, 1, 1), tangents_enabled);
	ZN_ASSERT_RETURN_V(mesh.is_valid(), Ref<Mesh>());

	mesh->surface_set_material(0, _fluid->get_material());

	return mesh;
}

namespace blocky {

void bake_fluid_model(
		const VoxelBlockyModelFluid &fluid_model,
		BakedModel &baked_model,
		StdVector<Ref<VoxelBlockyFluid>> &indexed_fluids,
		StdVector<BakedFluid> &baked_fluids,
		MaterialIndexer &materials
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

	BakedFluid &baked_fluid = baked_fluids[fluid_index];

	// TODO Allow more than one model with the same level?
	const int level = fluid_model.get_level();
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

	baked_model.model.surface_count = 1;
	baked_model.model.surfaces[0].material_id = materials.get_or_create_index(fluid->get_material());
}

} // namespace blocky

void VoxelBlockyModelFluid::bake(blocky::ModelBakingContext &ctx) const {
	blocky::bake_fluid_model(*this, ctx.model, ctx.indexed_fluids, ctx.baked_fluids, ctx.material_indexer);
}

void VoxelBlockyModelFluid::_on_fluid_changed() {
	emit_changed();
}

#ifdef TOOLS_ENABLED

void VoxelBlockyModelFluid::get_configuration_warnings(PackedStringArray &warnings) const {
	using namespace zylann::godot;

	if (!_fluid.is_valid()) {
		warnings.append(String("{0} has no fluid assigned.").format(varray(VoxelBlockyModelFluid::get_class_static())));
	}
}

#endif

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

	BIND_CONSTANT(MAX_LEVELS);
}

} // namespace zylann::voxel
