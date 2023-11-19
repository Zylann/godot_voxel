#include "voxel_blocky_library.h"
#include "../../constants/voxel_string_names.h"
#ifdef ZN_GODOT_EXTENSION
// For `MAKE_RESOURCE_TYPE_HINT`
#include "../../util/godot/classes/object.h"
#endif
#include "../../util/godot/classes/time.h"
#include "../../util/godot/core/array.h"
#include "../../util/godot/core/string.h"
#include "../../util/io/log.h"
#include "../../util/math/conv.h"
#include "../../util/profiling.h"
#include "../../util/string_funcs.h"
#include "voxel_blocky_model_cube.h"
#include "voxel_blocky_model_empty.h"
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

void VoxelBlockyLibrary::bake() {
	ZN_PROFILE_SCOPE();

	RWLockWrite lock(_baked_data_rw_lock);

	const uint64_t time_before = Time::get_singleton()->get_ticks_usec();

	// This is the only place we modify the data.

	_indexed_materials.clear();
	VoxelBlockyModel::MaterialIndexer materials{ _indexed_materials };

	_baked_data.models.resize(_voxel_models.size());
	for (size_t i = 0; i < _voxel_models.size(); ++i) {
		Ref<VoxelBlockyModel> config = _voxel_models[i];
		if (config.is_valid()) {
			config->bake(_baked_data.models[i], _bake_tangents, materials);
		} else {
			_baked_data.models[i].clear();
		}
	}

	_baked_data.indexed_materials_count = _indexed_materials.size();

	generate_side_culling_matrix(_baked_data);

	uint64_t time_spent = Time::get_singleton()->get_ticks_usec() - time_before;
	ZN_PRINT_VERBOSE(
			format("Took {} us to bake VoxelLibrary, indexed {} materials", time_spent, _indexed_materials.size()));

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
	std::vector<int> null_indices;

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
						.format(varray(VoxelBlockyLibrary::get_class_static(), VoxelBlockyModel::get_class_static())));
	}

	if (null_indices.size() > 0) {
		const String indices_str = join_comma_separated<int>(to_span(null_indices));
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

	ClassDB::bind_method(D_METHOD("add_model"), &VoxelBlockyLibrary::add_model);

	ClassDB::bind_method(D_METHOD("get_model", "index"), &VoxelBlockyLibrary::_b_get_model);
	ClassDB::bind_method(D_METHOD("get_model_index_from_resource_name", "name"),
			&VoxelBlockyLibrary::get_model_index_from_resource_name);

	// Legacy
	ClassDB::bind_method(D_METHOD("get_voxel_index_from_name", "name"),
			&VoxelBlockyLibrary::_b_deprecated_get_voxel_index_from_name);

	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "models", PROPERTY_HINT_ARRAY_TYPE,
						 MAKE_RESOURCE_TYPE_HINT(VoxelBlockyModel::get_class_static())),
			"set_models", "get_models");
}

} // namespace zylann::voxel
