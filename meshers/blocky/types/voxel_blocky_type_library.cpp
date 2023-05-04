#include "voxel_blocky_type_library.h"
#include "../../../constants/voxel_string_names.h"
#include "../../../util/godot/classes/time.h"
#include "../../../util/godot/core/string.h"
#include "../../../util/godot/core/typed_array.h"
#include "../../../util/profiling.h"
#include "../../../util/string_funcs.h"
#include "../voxel_blocky_model_cube.h"

namespace zylann::voxel {

void VoxelBlockyTypeLibrary::clear() {
	_types.clear();
}

void VoxelBlockyTypeLibrary::load_default() {
	clear();

	Ref<VoxelBlockyType> air_type;
	air_type.instantiate();
	air_type->set_unique_name(VoxelStringNames::get_singleton().air);

	Ref<VoxelBlockyModelCube> cube;
	cube.instantiate();

	Ref<VoxelBlockyType> solid_type;
	solid_type.instantiate();
	solid_type->set_base_model(cube);
	solid_type->set_unique_name(VoxelStringNames::get_singleton().cube);

	_types.push_back(air_type);
	_types.push_back(solid_type);

	_needs_baking = true;
}

void VoxelBlockyTypeLibrary::bake() {
	ZN_PROFILE_SCOPE();

	RWLockWrite lock(_baked_data_rw_lock);

	const uint64_t time_before = Time::get_singleton()->get_ticks_usec();

	// This is the only place we modify the data.

	_indexed_materials.clear();
	_baked_data.models.clear();

	std::vector<VoxelBlockyModel::BakedData> baked_models;
	std::vector<VoxelBlockyType::VariantKey> keys;
	VoxelBlockyModel::MaterialIndexer material_indexer{ _indexed_materials };

	for (size_t i = 0; i < _types.size(); ++i) {
		Ref<VoxelBlockyType> type = _types[i];
		ZN_ASSERT_CONTINUE_MSG(
				type.is_valid(), format("{} at index {} is null", VoxelBlockyType::get_class_static(), i));

		type->bake(baked_models, keys, material_indexer);

		// TODO Allocate IDs taking an IDMap into account for determinism
		for (VoxelBlockyModel::BakedData &baked_model : baked_models) {
			_baked_data.models.push_back(std::move(baked_model));
		}
		baked_models.clear();
		keys.clear();
	}

	_baked_data.indexed_materials_count = _indexed_materials.size();

	generate_side_culling_matrix(_baked_data);

	const uint64_t time_spent = Time::get_singleton()->get_ticks_usec() - time_before;
	ZN_PRINT_VERBOSE(
			format("Took {} us to bake VoxelLibrary, indexed {} materials", time_spent, _indexed_materials.size()));
}

void VoxelBlockyTypeLibrary::get_configuration_warnings(PackedStringArray &out_warnings) const {
	ZN_PROFILE_SCOPE();

	// Check null indices
	std::vector<unsigned int> null_indices;
	for (unsigned int i = 0; i < _types.size(); ++i) {
		if (_types[i].is_null()) {
			null_indices.push_back(i);
		}
	}
	if (null_indices.size() > 0) {
		const String null_indices_str = join_comma_separated<unsigned int>(to_span(null_indices));
		out_warnings.push_back(
				String("{0} contains null items at indices {1}").format(varray(get_class(), null_indices_str)));
	}

	// Check duplicate names
	struct DuplicateName {
		String name;
		unsigned int index1;
		unsigned int index2;
	};
	std::vector<DuplicateName> duplicate_names;
	for (unsigned int i = 0; i < _types.size(); ++i) {
		const Ref<VoxelBlockyType> &type1 = _types[i];
		if (type1.is_null()) {
			continue;
		}
		const StringName type1_name = type1->get_unique_name();
		for (unsigned int j = i + 1; j < _types.size(); ++j) {
			const Ref<VoxelBlockyType> &type2 = _types[i];
			if (type2.is_null()) {
				continue;
			}
			if (type2->get_unique_name() == type1_name) {
				duplicate_names.push_back(DuplicateName{ String(type1_name), i, j });
			}
		}
	}
	if (duplicate_names.size() > 0) {
		String message = String("{0} contains items with the same name: ").format(varray(get_class()));
		for (const DuplicateName &dn : duplicate_names) {
			message += String("{0} at indices {1} and {2}; ").format(varray(dn.name, dn.index1, dn.index2));
		}
		out_warnings.push_back(message);
	}

	// Check types
	unsigned int type_index = 0;
	for (const Ref<VoxelBlockyType> &type : _types) {
		if (type.is_null()) {
			continue;
		}

		String sname = String(type->get_unique_name()).strip_edges();
		if (sname.length() == 0) {
			out_warnings.push_back(String("{0} at index {1} has an empty name.")
										   .format(varray(VoxelBlockyType::get_class_static(), type_index)));
		}

		type->get_configuration_warnings(out_warnings);

		++type_index;
	}

	// TODO Check inconsistent attributes across types?
	// Currently, two attributes with the same name on two different types can have completely different values or
	// meaning. This is probably not a good idea.
}

int VoxelBlockyTypeLibrary::get_model_index_default(StringName type_name) const {
	Ref<VoxelBlockyType> type = get_type_from_name(type_name);
	if (type.is_null()) {
		return -1;
	}

	FixedArray<Ref<VoxelBlockyAttribute>, VoxelBlockyType::MAX_ATTRIBUTES> attribs;

	Span<const Ref<VoxelBlockyAttribute>> unordered_attribs = type->get_attributes();
	for (unsigned int i = 0; i < unordered_attribs.size(); ++i) {
		attribs[i] = unordered_attribs[i];
	}

	VoxelBlockyAttribute::sort_by_name(Span<Ref<VoxelBlockyAttribute>>(attribs.data(), unordered_attribs.size()));

	VoxelID id;
	id.type_name = type_name;

	for (unsigned int i = 0; i < unordered_attribs.size(); ++i) {
		id.variant_key.attribute_names[i] = attribs[i]->get_attribute_name();
		id.variant_key.attribute_values[i] = attribs[i]->get_default_value();
	}

	return get_model_index(id);
}

static bool parse_attribute_value(
		const Variant &vv, const VoxelBlockyType &type, const StringName &attrib_name, uint8_t &out_attrib_value) {
	if (vv.get_type() == Variant::STRING_NAME) {
		StringName value_name = vv;

		Ref<VoxelBlockyAttribute> attrib = type.get_attribute_by_name(attrib_name);
		ERR_FAIL_COND_V_MSG(attrib.is_null(), -1,
				String("{0} type '{1}' has no attribute named '{2}'")
						.format(varray(VoxelBlockyType::get_class_static(), type.get_unique_name(), attrib_name)));

		int attrib_value = attrib->get_value_from_name(value_name);
		ERR_FAIL_COND_V_MSG(attrib_value == -1, -1,
				String("{0} ('{1}') has no value with name '{2}'")
						.format(varray(attrib->get_class(), attrib->get_attribute_name(), value_name)));

		out_attrib_value = attrib_value;

	} else if (vv.get_type() == Variant::INT) {
		const int raw_value = vv;
		ERR_FAIL_COND_V_MSG(raw_value >= 0, -1, "Attribute integer value cannot be negative.");
		out_attrib_value = raw_value;

	} else if (vv.get_type() == Variant::BOOL) {
		const bool boolean_value = vv;
		out_attrib_value = boolean_value ? 1 : 0;

	} else {
		ZN_PRINT_ERROR("Expected StringName or integer for attribute value");
		return false;
	}

	return true;
}

int VoxelBlockyTypeLibrary::get_model_index_single_attribute(StringName type_name, Variant p_attrib_value) const {
	ZN_PROFILE_SCOPE();

	Ref<VoxelBlockyType> type = get_type_from_name(type_name);
	if (type.is_null()) {
		return -1;
	}

	Span<const Ref<VoxelBlockyAttribute>> attribs = type->get_attributes();
	ZN_ASSERT_RETURN_V_MSG(attribs.size() >= 1, -1, "The requested type has no attribute.");

	Ref<VoxelBlockyAttribute> attrib = attribs[0];
	ZN_ASSERT_RETURN_V(attrib.is_valid(), -1);
	const StringName attrib_name = attrib->get_attribute_name();

	uint8_t attrib_value;
	if (!parse_attribute_value(p_attrib_value, **type, attrib_name, attrib_value)) {
		return -1;
	}

	VoxelID id;
	id.type_name = type_name;
	id.variant_key.attribute_names[0] = attrib_name;
	id.variant_key.attribute_values[0] = attrib_value;

	return get_model_index(id);
}

int VoxelBlockyTypeLibrary::get_model_index_with_attributes(StringName type_name, Array attribs_spec) const {
	ZN_PROFILE_SCOPE();

	VoxelID id;
	id.type_name = type_name;

	Ref<VoxelBlockyType> type = get_type_from_name(type_name);
	if (type.is_null()) {
		return -1;
	}

	ZN_ASSERT_RETURN_V_MSG((attribs_spec.size() % 2) == 0, -1,
			"Array of attributes must contain series of attribute names and values. The provided array has an uneven "
			"size.");

	const unsigned int attribute_count = attribs_spec.size() / 2;

	FixedArray<std::pair<StringName, uint8_t>, VoxelBlockyType::MAX_ATTRIBUTES> unordered_key;

	// Parse attributes
	for (int attrib_spec_index = 0; attrib_spec_index < attribs_spec.size(); attrib_spec_index += 2) {
		Variant kv = attribs_spec[attrib_spec_index];
		Variant vv = attribs_spec[attrib_spec_index + 1];

		std::pair<StringName, uint8_t> pair;

		ZN_ASSERT_RETURN_V_MSG(kv.get_type() != Variant::STRING_NAME, -1, "Attribute name must be a StringName.");
		StringName attrib_name = kv;
		pair.first = attrib_name;

		if (!parse_attribute_value(vv, **type, attrib_name, pair.second)) {
			return -1;
		}

		unordered_key[attrib_spec_index / 2] = pair;
	}

	// Sort, because a set of attributes have a fixed order so they can be looked up more efficiently
	VoxelBlockyAttribute::sort_by_name(Span<std::pair<StringName, uint8_t>>(unordered_key.data(), attribute_count));

	for (unsigned int i = 0; i < attribute_count; ++i) {
		id.variant_key.attribute_names[i] = unordered_key[i].first;
		id.variant_key.attribute_values[i] = unordered_key[i].second;
	}

	return get_model_index(id);
}

int VoxelBlockyTypeLibrary::get_model_index(const VoxelID queried_id) const {
	// Not optimized, we'll see if it needs to be a HashMap or other
	for (unsigned int i = 0; i < _id_map.size(); ++i) {
		const VoxelID &id = _id_map[i];
		if (id == queried_id) {
			return i;
		}
	}
	return -1;
}

Ref<VoxelBlockyType> VoxelBlockyTypeLibrary::get_type_from_name(StringName name) const {
	// Not optimized, we'll see if it needs to be a HashMap or other
	for (const Ref<VoxelBlockyType> &type : _types) {
		if (type.is_valid() && type->get_unique_name() == name) {
			return type;
		}
	}
	return Ref<VoxelBlockyType>();
}

// TODO
// Ref<VoxelBlockyType> VoxelBlockyTypeLibrary::get_type_from_model_index(int i) const {
// 	return Ref<VoxelBlockyType>();
// }

TypedArray<VoxelBlockyType> VoxelBlockyTypeLibrary::_b_get_types() const {
	return to_typed_array(to_span(_types));
}

void VoxelBlockyTypeLibrary::_b_set_types(TypedArray<VoxelBlockyType> types) {
	copy_to(_types, types);
	_needs_baking = true;
}

void VoxelBlockyTypeLibrary::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_types"), &VoxelBlockyTypeLibrary::_b_get_types);
	ClassDB::bind_method(D_METHOD("set_types"), &VoxelBlockyTypeLibrary::_b_set_types);

	ClassDB::bind_method(D_METHOD("bake"), &VoxelBlockyTypeLibrary::bake);

	ClassDB::bind_method(
			D_METHOD("get_model_index_default", "type_name"), &VoxelBlockyTypeLibrary::get_model_index_default);
	ClassDB::bind_method(D_METHOD("get_model_index_single_attribute", "type_name", "attrib_value"),
			&VoxelBlockyTypeLibrary::get_model_index_single_attribute);
	ClassDB::bind_method(D_METHOD("get_model_index_with_attributes", "type_name", "attrib_specs"),
			&VoxelBlockyTypeLibrary::get_model_index_with_attributes);

	ClassDB::bind_method(D_METHOD("get_type_from_name", "type_name"), &VoxelBlockyTypeLibrary::get_type_from_name);

	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "types", PROPERTY_HINT_ARRAY_TYPE,
						 MAKE_RESOURCE_TYPE_HINT(VoxelBlockyType::get_class_static())),
			"set_types", "get_types");
}

} // namespace zylann::voxel
