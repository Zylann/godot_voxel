#include "voxel_blocky_library.h"
#include "../../constants/voxel_string_names.h"
#ifdef ZN_GODOT_EXTENSION
// For `MAKE_RESOURCE_TYPE_HINT`
#include "../../util/godot/classes/object.h"
#endif
#include "../../util/containers/container_funcs.h"
#include "../../util/godot/classes/time.h"
#include "../../util/godot/core/array.h"
#include "../../util/godot/core/string.h"
#include "../../util/io/log.h"
#include "../../util/math/conv.h"
#include "../../util/math/funcs.h"
#include "../../util/profiling.h"
#include "../../util/string/format.h"
#include "voxel_blocky_model_cube.h"
#include "voxel_blocky_model_empty.h"
#include "voxel_blocky_model_fluid.h"
#include "voxel_blocky_model_mesh.h"

#include <bitset>

namespace zylann::voxel {

VoxelBlockyLibrary::VoxelBlockyLibrary() {}

VoxelBlockyLibrary::~VoxelBlockyLibrary() {}

void VoxelBlockyLibrary::clear() {
	_voxel_models.clear();
}

void VoxelBlockyLibrary::load_default() {
	clear();

	Ref<VoxelBlockyModelMesh> air;
	air.instantiate();
	air->set_name("air");

	Ref<VoxelBlockyModelCube> cube;
	cube.instantiate();
	cube->set_name("cube");

	_voxel_models.push_back(air);
	_voxel_models.push_back(cube);

	_needs_baking = true;
}

void bake_fluid_model(
		const VoxelBlockyModelFluid &fluid_model,
		uint16_t model_index,
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
	if (level >= baked_fluid.level_model_indices.size()) {
		baked_fluid.level_model_indices.resize(level + 1, VoxelBlockyModel::AIR_ID);
	}
	baked_fluid.level_model_indices[level] = model_index;
	baked_model.fluid_level = level;

	baked_model.transparency_index = fluid_model.get_transparency_index();
	baked_model.culls_neighbors = fluid_model.get_culls_neighbors();
	baked_model.color = fluid_model.get_color();
	baked_model.is_random_tickable = fluid_model.is_random_tickable();
	baked_model.box_collision_mask = fluid_model.get_collision_mask();
	// baked_model.box_collision_aabbs = fluid_model.get_collision_aabbs();

	// This is to be decided dynamically. The top side is always empty.
	baked_model.model.empty_sides_mask = (1 << Cube::SIDE_POSITIVE_Y);

	// TODO Specify material
	// Assign material overrides if any
	// for (unsigned int surface_index = 0; surface_index < model.surface_count; ++surface_index) {
	// 	if (surface_index < _surface_count) {
	// 		const SurfaceParams &surface_params = _surface_params[surface_index];
	// 		const Ref<Material> material = surface_params.material_override;

	// 		BakedData::Surface &surface = model.surfaces[surface_index];

	// 		const unsigned int material_index = materials.get_or_create_index(material);
	// 		surface.material_id = material_index;

	// 		surface.collision_enabled = surface_params.collision_enabled;
	// 	}
	// }
}

void bake_fluid(
		const VoxelBlockyFluid &fluid,
		VoxelBlockyFluid::BakedData &baked_fluid,
		VoxelBlockyModel::MaterialIndexer &materials
) {
	for (const uint16_t model_index : baked_fluid.level_model_indices) {
		if (model_index == VoxelBlockyModel::AIR_ID) {
			ZN_PRINT_ERROR("Fluid is missing some levels");
			break;
		}
	}

	if (baked_fluid.level_model_indices.size() == 1) {
		ZN_PRINT_ERROR("Fluid with only one level will not work properly");
	}

	Ref<Material> material = fluid.get_material();
	baked_fluid.material_id = materials.get_or_create_index(material);

	// TODO This part shouldn't be necessary? it's the same for every fluid
	for (unsigned int side_index = 0; side_index < Cube::SIDE_COUNT; ++side_index) {
		make_cube_side_vertices_tangents(
				baked_fluid.side_surfaces[side_index], side_index, VoxelBlockyFluid::BakedData::TOP_HEIGHT, false
		);
	}

	for (unsigned int side_index = 0; side_index < Cube::SIDE_COUNT; ++side_index) {
		VoxelBlockyModel::BakedData::SideSurface &side_surface = baked_fluid.side_surfaces[side_index];

		// TODO Bake UVs with animation info somehow
		side_surface.uvs.resize(4);
		for (unsigned int i = 0; i < side_surface.uvs.size(); ++i) {
			side_surface.uvs[i] = Vector2f();
		}
	}
}

void VoxelBlockyLibrary::bake() {
	ZN_PROFILE_SCOPE();

	RWLockWrite lock(_baked_data_rw_lock);

	const uint64_t time_before = Time::get_singleton()->get_ticks_usec();

	// This is the only place we modify the data.

	_indexed_materials.clear();
	VoxelBlockyModel::MaterialIndexer materials{ _indexed_materials };

	StdVector<Ref<VoxelBlockyFluid>> indexed_fluids;

	_baked_data.models.resize(_voxel_models.size());
	for (size_t i = 0; i < _voxel_models.size(); ++i) {
		Ref<VoxelBlockyModel> config = _voxel_models[i];
		VoxelBlockyModel::BakedData &baked_model = _baked_data.models[i];

		// TODO Perhaps models need a broader context to be baked
		Ref<VoxelBlockyModelFluid> fluid_model = config;
		if (!fluid_model.is_valid()) {
			baked_model.fluid_index = VoxelBlockyModel::NULL_FLUID_INDEX;
		} else {
			bake_fluid_model(**fluid_model, i, baked_model, indexed_fluids, _baked_data.fluids);
			continue;
		}

		if (config.is_valid()) {
			config->bake(baked_model, _bake_tangents, materials);

		} else {
			baked_model.clear();
		}
	}

	for (unsigned int fluid_index = 0; fluid_index < indexed_fluids.size(); ++fluid_index) {
		const VoxelBlockyFluid &fluid = **indexed_fluids[fluid_index];
		VoxelBlockyFluid::BakedData &baked_fluid = _baked_data.fluids[fluid_index];
		bake_fluid(fluid, baked_fluid, materials);
	}

	_baked_data.indexed_materials_count = _indexed_materials.size();

	generate_side_culling_matrix(_baked_data);

	uint64_t time_spent = Time::get_singleton()->get_ticks_usec() - time_before;
	ZN_PRINT_VERBOSE(
			format("Took {} us to bake VoxelLibrary, indexed {} materials", time_spent, _indexed_materials.size())
	);

	_needs_baking = false;
}

int VoxelBlockyLibrary::get_model_index_from_resource_name(String resource_name) const {
	for (unsigned int i = 0; i < _voxel_models.size(); ++i) {
		const Ref<VoxelBlockyModel> &model = _voxel_models[i];
		if (model.is_valid() && model->get_name() == resource_name) {
			return i;
		}
	}
	return -1;
}

int VoxelBlockyLibrary::add_model(Ref<VoxelBlockyModel> model) {
	const int index = _voxel_models.size();
	_voxel_models.push_back(model);
	_needs_baking = true;
	return index;
}

// unsigned int VoxelBlockyLibrary::get_model_count() const {
// 	return _voxel_models.size();
// }

bool VoxelBlockyLibrary::_set(const StringName &p_name, const Variant &p_value) {
	// Legacy

	String property_name(p_name);
	if (property_name.begins_with("voxels/")) {
		unsigned int idx = property_name.get_slicec('/', 1).to_int();
		Ref<VoxelBlockyModel> legacy_model = p_value;

		if (legacy_model.is_valid()) {
			// Convert old classes into new classes. This is why `VoxelBlockyModel` is unfortunately not abstract, even
			// if it should be...

			const VoxelBlockyModel::LegacyProperties &legacy_properties = legacy_model->get_legacy_properties();

			Ref<VoxelBlockyModel> new_model;

			if (legacy_properties.geometry_type == VoxelBlockyModel::LegacyProperties::GEOMETRY_CUBE) {
				Ref<VoxelBlockyModelCube> cube;
				cube.instantiate();
				cube->copy_base_properties_from(**legacy_model);
				for (unsigned int side = 0; side < VoxelBlockyModel::SIDE_COUNT; ++side) {
					cube->set_tile(VoxelBlockyModel::Side(side), to_vec2i(legacy_properties.cube_tiles[side]));
				}
				// TODO Can't guarantee that this will work, because Godot could set that property later.
				// It might actually work if Godot properly sorts properties in TSCN/TRES, because `voxels/*` starts
				// with `v`, which comes after `atlas_size`.
				cube->set_atlas_size_in_tiles(Vector2i(_legacy_atlas_size, _legacy_atlas_size));
				new_model = cube;

			} else if (legacy_properties.geometry_type == VoxelBlockyModel::LegacyProperties::GEOMETRY_MESH) {
				Ref<VoxelBlockyModelMesh> mesh_model;
				mesh_model.instantiate();
				mesh_model->copy_base_properties_from(**legacy_model);
				mesh_model->set_mesh(legacy_properties.custom_mesh);
				new_model = mesh_model;

			} else if (legacy_properties.geometry_type == VoxelBlockyModel::LegacyProperties::GEOMETRY_NONE) {
				Ref<VoxelBlockyModelEmpty> empty_model;
				empty_model.instantiate();
				new_model = empty_model;
			}

			if (idx >= _voxel_models.size()) {
				_voxel_models.resize(idx + 1);
			}
			if (new_model.is_valid()) {
				new_model->set_name(legacy_properties.name);
				_voxel_models[idx] = new_model;
			}
		}

		return true;

	} else if (property_name == "atlas_size") {
		_legacy_atlas_size = p_value;
		return true;
	}

	return false;
}

#ifdef TOOLS_ENABLED

void VoxelBlockyLibrary::get_configuration_warnings(PackedStringArray &out_warnings) const {
	StdVector<int> null_indices;

	bool has_solid_model = false;
	for (unsigned int i = 0; i < _voxel_models.size() && !has_solid_model; ++i) {
		Ref<VoxelBlockyModel> model = _voxel_models[i];
		if (model.is_null()) {
			null_indices.push_back(i);
			continue;
		}
		if (!model->is_empty()) {
			has_solid_model = true;
			break;
		}
	}
	if (!has_solid_model) {
		out_warnings.append(
				String(ZN_TTR("The {0} only has empty {1}s."))
						.format(varray(VoxelBlockyLibrary::get_class_static(), VoxelBlockyModel::get_class_static()))
		);
	}

	if (null_indices.size() > 0) {
		const String indices_str = godot::join_comma_separated<int>(to_span(null_indices));
		// Should we really consider it a problem?
		out_warnings.append(String(ZN_TTR("The {0} has null model entries: {1}"))
									.format(varray(VoxelBlockyLibrary::get_class_static(), indices_str)));
	}
}

#endif

Ref<VoxelBlockyModel> VoxelBlockyLibrary::_b_get_model(unsigned int id) const {
	ERR_FAIL_COND_V(id >= _voxel_models.size(), Ref<VoxelBlockyModel>());
	return _voxel_models[id];
}

TypedArray<VoxelBlockyModel> VoxelBlockyLibrary::_b_get_models() const {
	TypedArray<VoxelBlockyModel> array;
	array.resize(_voxel_models.size());
	for (size_t i = 0; i < _voxel_models.size(); ++i) {
		array[i] = _voxel_models[i];
	}
	return array;
}

void VoxelBlockyLibrary::_b_set_models(TypedArray<VoxelBlockyModel> models) {
	const size_t prev_size = _voxel_models.size();
	_voxel_models.resize(models.size());
	for (int i = 0; i < models.size(); ++i) {
		_voxel_models[i] = models[i];
	}
	_needs_baking = (_voxel_models.size() != prev_size);
}

int VoxelBlockyLibrary::_b_deprecated_get_voxel_index_from_name(String p_name) const {
	WARN_DEPRECATED_MSG("Use `get_model_index_from_resource_name` instead.");
	return get_model_index_from_resource_name(p_name);
}

void VoxelBlockyLibrary::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_models"), &VoxelBlockyLibrary::_b_get_models);
	ClassDB::bind_method(D_METHOD("set_models"), &VoxelBlockyLibrary::_b_set_models);

	ClassDB::bind_method(D_METHOD("add_model", "model"), &VoxelBlockyLibrary::add_model);

	ClassDB::bind_method(D_METHOD("get_model", "index"), &VoxelBlockyLibrary::_b_get_model);
	ClassDB::bind_method(
			D_METHOD("get_model_index_from_resource_name", "name"),
			&VoxelBlockyLibrary::get_model_index_from_resource_name
	);

	// Legacy
	ClassDB::bind_method(
			D_METHOD("get_voxel_index_from_name", "name"), &VoxelBlockyLibrary::_b_deprecated_get_voxel_index_from_name
	);

	ADD_PROPERTY(
			PropertyInfo(
					Variant::ARRAY,
					"models",
					PROPERTY_HINT_ARRAY_TYPE,
					MAKE_RESOURCE_TYPE_HINT(VoxelBlockyModel::get_class_static())
			),
			"set_models",
			"get_models"
	);
}

} // namespace zylann::voxel
