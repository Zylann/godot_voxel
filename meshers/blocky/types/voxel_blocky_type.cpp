#include "voxel_blocky_type.h"
#include "../../../constants/voxel_string_names.h"
#include "../../../util/containers/container_funcs.h"
#include "../../../util/godot/classes/engine.h"
#include "../../../util/godot/classes/ref_counted.h"
#include "../../../util/godot/core/array.h"
#include "../../../util/godot/core/string.h"
#include "../../../util/godot/core/typed_array.h"
#ifdef ZN_GODOT_EXTENSION
// For `MAKE_RESOURCE_TYPE_HINT`
#include "../../../util/godot/classes/object.h"
#endif
#include "../../../util/math/ortho_basis.h"
#include "../../../util/profiling.h"
#include "../../../util/string/format.h"
#include "../voxel_blocky_library_base.h"

namespace zylann::voxel {

// Making types can get quite complicated, config files sound like a better solution compared to messing around in the
// inspector, see how Minecraft defines their models: https://minecraft.wiki/w/Tutorials/Models

VoxelBlockyType::VoxelBlockyType() {
	_name = VoxelStringNames::get_singleton().unnamed;
}

void VoxelBlockyType::set_unique_name(StringName p_name) {
	if (p_name != _name) {
		_name = p_name;
		// Also set resource name, so Godot will use it in array inspector
		set_name(p_name);
		emit_changed();
	}
}

StringName VoxelBlockyType::get_unique_name() const {
	return _name;
}

/*
template <typename Observable_T>
void update_editor_signal(Ref<Observable_T> prev_observable, Ref<Observable_T> new_observable, StringName signal_name,
		Callable signal_handler) {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		if (prev_observable.is_valid()) {
			prev_observable->disconnect(signal_name, signal_handler);
		}
		if (new_observable.is_valid()) {
			new_observable->connect(signal_name, signal_handler);
		}
	}
#endif
}
*/

void VoxelBlockyType::set_base_model(Ref<VoxelBlockyModel> model) {
	if (model == _base_model) {
		return;
	}
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		Callable change_handler = callable_mp(this, &VoxelBlockyType::_on_base_model_changed);
		if (_base_model.is_valid()) {
			_base_model->disconnect(VoxelStringNames::get_singleton().changed, change_handler);
		}
		if (model.is_valid()) {
			model->connect(VoxelStringNames::get_singleton().changed, change_handler);
		}
	}
#endif

	_base_model = model;
	emit_changed();
}

Ref<VoxelBlockyModel> VoxelBlockyType::get_base_model() const {
	return _base_model;
}

Span<const Ref<VoxelBlockyAttribute>> VoxelBlockyType::get_attributes() const {
	return to_span(_attributes);
}

Ref<VoxelBlockyAttribute> VoxelBlockyType::get_attribute_by_name(const StringName &attrib_name) const {
	for (const Ref<VoxelBlockyAttribute> &attrib : _attributes) {
		if (attrib.is_valid() && attrib->get_attribute_name() == attrib_name) {
			return attrib;
		}
	}
	return Ref<VoxelBlockyAttribute>();
}

Ref<VoxelBlockyAttribute> VoxelBlockyType::get_rotation_attribute() const {
	Ref<VoxelBlockyAttribute> rotation_attribute;
	for (const Ref<VoxelBlockyAttribute> &attrib : _attributes) {
		if (attrib.is_valid() && attrib->is_rotation()) {
			rotation_attribute = attrib;
			break;
		}
	}
	return rotation_attribute;
}

void VoxelBlockyType::get_checked_attributes(StdVector<Ref<VoxelBlockyAttribute>> &out_attribs) const {
	gather_and_sort_attributes(_attributes, out_attribs);
}

/*static bool parse_variant_key_from_property_name(const String &property_name, VoxelBlockyType::VariantKey &out_key) {
	Span<const char32_t> str(property_name.ptr(), property_name.length());

	// `variants/attrib1_value1_attrib2_value2`

	unsigned int i = 0;
	for (const char32_t c : str) {
		++i;
		if (c == '/') {
			break;
		}
	}
	ZN_ASSERT_RETURN_V(i > 0, false);

	VoxelBlockyType::VariantKey key;
	unsigned int attrib_index = 0;

	while (i < str.size()) {
		ZN_ASSERT_RETURN_V_MSG(attrib_index < VoxelBlockyType::MAX_ATTRIBUTES, false,
				format("When parsing property name '{}'", property_name));

		unsigned int attrib_start = i;

		for (; i < str.size(); ++i) {
			const char32_t c = str[i];
			if (c == '_') {
				break;
			}
		}

		unsigned int attrib_end = i;

		String attrib_name = property_name.substr(attrib_start, attrib_end - attrib_start);
		key.attribute_names[attrib_index] = attrib_name;

		++i;
		ZN_ASSERT_RETURN_V_MSG(i < str.size(), false, format("When parsing property name '{}'", property_name));

		unsigned int value = 0;
		for (; i < str.size(); ++i) {
			const char32_t c = str[i];
			if (c >= '0' && c <= '9') {
				value *= 10;
				value += (c - '0');
			} else if (c == '_') {
				++i;
				break;
			} else {
				// TODO This happens when an attribute contains an underscore in its name (which is legit!).
				// This demonstrates we can't keep hacking property names for this... it becomes REALLY annoying to
				// parse this. It's also really annoying for the user to recognize the names.
				// We should make a custom editor for this, but by doing that, we would loose sub-inspectors...
				// so the only way to keep a good Godot integration is to make an entirely dedicated editor for types,
				// so THE (glorious singleton) Godot inspector can be visible at the same time to edit sub-resources...
				ZN_PRINT_ERROR(format(
						"Unexpected character at position {} when parsing variant property '{}'", i, property_name));
				return false;
			}
		}

		key.attribute_values[attrib_index] = value;
		++attrib_index;
	}

	out_key = key;
	return true;
}*/

void VoxelBlockyType::set_variant(const VariantKey &key, Ref<VoxelBlockyModel> model) {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		Ref<VoxelBlockyModel> prev_model = get_variant(key);
		Callable change_handler = callable_mp(this, &VoxelBlockyType::_on_base_model_changed);
		if (prev_model.is_valid()) {
			prev_model->disconnect(VoxelStringNames::get_singleton().changed, change_handler);
		}
		if (model.is_valid()) {
			model->connect(VoxelStringNames::get_singleton().changed, change_handler);
		}
	}
#endif

	for (VariantData &vd : _variants) {
		if (vd.key == key && vd.model != model) {
			// TODO If the model is null, remove it from the list
			const bool changed = (vd.model != model);
			vd.model = model;
			if (changed) {
				emit_changed();
			}
			return;
		}
	}
	VariantData new_vd;
	new_vd.key = key;
	new_vd.model = model;
	_variants.push_back(new_vd);
	emit_changed();
}

Ref<VoxelBlockyModel> VoxelBlockyType::get_variant(const VariantKey &key) const {
	for (const VariantData &vd : _variants) {
		if (vd.key == key) {
			return vd.model;
		}
	}
	return Ref<VoxelBlockyModel>();
}

/*bool VoxelBlockyType::_set(const StringName &p_name, const Variant &p_value) {
	ZN_PROFILE_SCOPE();
	String name_str = p_name;

	if (name_str.begins_with("variants/")) {
		VariantKey key;
		if (parse_variant_key_from_property_name(name_str, key)) {
			set_variant(key, p_value);
			return true;
		}
	}

	return false;
}*/

/*bool VoxelBlockyType::_get(const StringName &p_name, Variant &r_ret) const {
	ZN_PROFILE_SCOPE();
	String name_str = p_name;

	if (name_str.begins_with("variants/")) {
		VariantKey key;
		if (parse_variant_key_from_property_name(name_str, key)) {
			r_ret = get_variant(key);
			return true;
		}
	}

	return false;
}*/

/*void VoxelBlockyType::_get_property_list(List<PropertyInfo> *p_list) const {
	ZN_PROFILE_SCOPE();

	StdVector<Ref<VoxelBlockyAttribute>> attributes;
	gather_and_sort_attributes(_attributes, attributes);

	StdVector<VariantKey> keys;
	generate_keys(attributes, keys, !_automatic_rotations);

	// Only show variants if there are more than one. If there is only one, it's just the base model (or there is only
	// one attribute and it's a rotation and automatic rotations are enabled).
	if (keys.size() > 1) {
		for (const VariantKey &key : keys) {
			String property_name = "variants/";
			for (unsigned int i = 0; i < attributes.size(); ++i) {
				if (i > 0) {
					property_name += "_";
				}
				property_name += key.attribute_names[i];
				property_name += "_";
				property_name += String::num_int64(key.attribute_values[i]);
			}
			p_list->push_back(PropertyInfo(
					Variant::OBJECT, property_name, PROPERTY_HINT_RESOURCE_TYPE, VoxelBlockyModel::get_class_static()));
		}
	}
}*/

namespace {

// Get automatic rotation transform to apply to a model when baking.
// It is based on the assumption the base model is pre-rotated according to the default value of the rotation attribute
// (which could be identity, most of the time). So we essentially need to obtain the transformation that goes from the
// default rotation to others.
math::OrthoBasis get_baking_rotation_ortho_basis(
		Ref<VoxelBlockyAttribute> rotation_attribute, unsigned int rotation_attribute_value) {
	const unsigned int default_value = rotation_attribute->get_default_value();

	const unsigned int src_basis_index = rotation_attribute->get_ortho_rotation_index_from_value(default_value);
	const unsigned int dst_basis_index =
			rotation_attribute->get_ortho_rotation_index_from_value(rotation_attribute_value);

	const math::OrthoBasis src_basis = math::get_ortho_basis_from_index(src_basis_index);
	const math::OrthoBasis dst_basis = math::get_ortho_basis_from_index(dst_basis_index);

	return src_basis.inverted() * dst_basis;
}

// String variant_key_to_string(
// 		const VoxelBlockyType::VariantKey &key, const StdVector<Ref<VoxelBlockyAttribute>> &attributes) {
// 	String s;
// 	unsigned int i = 0;
// 	for (const Ref<VoxelBlockyAttribute> &attrib : attributes) {
// 		if (i > 0) {
// 			s += "_";
// 		}
// 		if (attrib.is_null()) {
// 			s += "<null>=";
// 		}
// 		s += String::num_int64(key.attribute_values[i]);
// 		++i;
// 	}
// 	return s;
// }

} // namespace

void VoxelBlockyType::bake(StdVector<VoxelBlockyModel::BakedData> &out_models, StdVector<VariantKey> &out_keys,
		VoxelBlockyModel::MaterialIndexer &material_indexer, const VariantKey *specific_key, bool bake_tangents) const {
	ZN_PROFILE_SCOPE();

	// Don't print warnings when used for previewing. It's ok to have momentarily invalid setups when the user is
	// editing properties.
	const bool print_warnings = (specific_key == nullptr);

	StdVector<Ref<VoxelBlockyAttribute>> attributes;
	gather_and_sort_attributes(_attributes, attributes);

	// Find rotation attribute, if any
	Ref<VoxelBlockyAttribute> rotation_attribute;
	unsigned int rotation_attribute_index = 0;
	for (const Ref<VoxelBlockyAttribute> &attrib : attributes) {
		if (attrib->is_rotation()) {
			rotation_attribute = attrib;
			break;
		}
		++rotation_attribute_index;
	}

	StdVector<VariantKey> keys;
	if (specific_key != nullptr) {
		// For previewing a single model
		keys.push_back(*specific_key);
	} else {
		generate_keys(attributes, keys, true);
	}

	unsigned int first_model_index = out_models.size();
	out_models.resize(out_models.size() + keys.size());
	Span<VoxelBlockyModel::BakedData> baked_models(&out_models[first_model_index], keys.size());

	for (unsigned int variant_index = 0; variant_index < baked_models.size(); ++variant_index) {
		VoxelBlockyModel::BakedData &baked_model = baked_models[variant_index];
		const VariantKey &key = keys[variant_index];

		Ref<VoxelBlockyModel> model = get_variant(key);

		if (model.is_valid()) {
			// Variant specified explicitely, just use it
			model->bake(baked_model, bake_tangents, material_indexer);

		} else if (_automatic_rotations && rotation_attribute.is_valid()) {
			// Not specified, but the type has a rotation attribute.
			// Assume rotation. Rotate from default.

			ZN_ASSERT_CONTINUE_MSG(
					key.attribute_names[rotation_attribute_index] == rotation_attribute->get_attribute_name(), "Bug?");

			// Pick reference model:
			// The model with default rotation must have been assigned.
			VariantKey ref_key = key;
			ref_key.attribute_values[rotation_attribute_index] = rotation_attribute->get_default_value();
			Ref<VoxelBlockyModel> ref_model = get_variant(ref_key);
			if (ref_model.is_null()) {
				// If not, use base model...
				if (_base_model.is_null()) {
					if (print_warnings) {
						// If base model is null... variant will be empty. Should be a configuration warning. If empty
						// is really desired, VoxelBlockyModelEmpty should be used.
						WARN_PRINT(String(
								"No model found for rotation variant ({0}) when baking {1} with name {2}. The model "
								"will be empty.")
										   .format(varray(ref_key.to_string(), get_class(), get_unique_name())));
					}
					continue;
				}
				ref_model = _base_model;
			}

			// Apply rotation
			const math::OrthoBasis trans_basis =
					get_baking_rotation_ortho_basis(rotation_attribute, key.attribute_values[rotation_attribute_index]);
			Ref<VoxelBlockyModel> temp_model = ref_model->duplicate();
			temp_model->rotate_ortho(trans_basis);
			temp_model->bake(baked_model, bake_tangents, material_indexer);

		} else {
			// No variant specified, use base model.
			if (_base_model.is_valid()) {
				_base_model->bake(baked_model, bake_tangents, material_indexer);
			} else if (print_warnings) {
				WARN_PRINT(
						String("No model found for variant {0} when baking {1} with name {2}. The model will be empty.")
								.format(varray(key.to_string(), get_class(), get_unique_name())));
			}
		}
	}

	if (_base_model.is_valid()) {
		for (VoxelBlockyModel::BakedData &baked_model : baked_models) {
			baked_model.color *= _base_model->get_color();
			baked_model.transparency_index = _base_model->get_transparency_index();
			baked_model.box_collision_mask |= _base_model->get_collision_mask();
			baked_model.is_random_tickable |= _base_model->is_random_tickable();
		}
	}

	out_keys = std::move(keys);
}

bool try_get_attribute_index_from_name(
		const StdVector<Ref<VoxelBlockyAttribute>> &attributes, const StringName &name, unsigned int &out_index) {
	for (unsigned int i = 0; i < attributes.size(); ++i) {
		const Ref<VoxelBlockyAttribute> &attrib = attributes[i];
		if (attrib.is_valid() && attrib->get_attribute_name() == name) {
			out_index = i;
			return true;
		}
	}
	return false;
}

template <typename T>
unsigned int get_non_null_count(const StdVector<T> &objects) {
	unsigned int count = 0;
	for (const T &obj : objects) {
		if (obj != nullptr) {
			++count;
		}
	}
	return count;
}

#ifdef TOOLS_ENABLED

void VoxelBlockyType::get_configuration_warnings(PackedStringArray &out_warnings) const {
	ZN_PROFILE_SCOPE();

	for (const Ref<VoxelBlockyAttribute> &attrib : _attributes) {
		if (attrib.is_null()) {
			out_warnings.push_back(
					String("{0} with name `{1}` has null attributes, consider removing them from the list")
							.format(varray(get_class(), get_unique_name())));
			break;
		}
	}

	StdVector<Ref<VoxelBlockyAttribute>> attributes;
	gather_and_sort_attributes(_attributes, attributes);

	for (const Ref<VoxelBlockyAttribute> &attrib : attributes) {
		ZN_ASSERT_RETURN(attrib.is_valid());
		attrib->get_configuration_warnings(out_warnings);
	}

	StdVector<VariantKey> keys;
	generate_keys(attributes, keys, false);

	unsigned int unspecified_keys_count = 0;
	for (const VariantKey &key : keys) {
		Ref<VoxelBlockyModel> model = get_variant(key);
		if (model.is_null()) {
			++unspecified_keys_count;
		}
	}

	if (unspecified_keys_count > 0) {
		out_warnings.push_back(String("{0} with name '{1}' has {2} unspecified variants.")
									   .format(varray(get_class(), get_unique_name(), unspecified_keys_count)));
	}
}

#endif

Ref<Mesh> VoxelBlockyType::get_preview_mesh(const VariantKey &key) const {
	StdVector<VoxelBlockyModel::BakedData> baked_models;
	StdVector<Ref<Material>> materials;
	VoxelBlockyModel::MaterialIndexer material_indexer{ materials };
	StdVector<VariantKey> keys;

	// Assuming tangents are needed, which might not always be the case, but we won't waste much for just a preview
	const bool require_tangents = true;

	bake(baked_models, keys, material_indexer, &key, true);

	ZN_ASSERT_RETURN_V(baked_models.size() == 1, Ref<Mesh>());
	const VoxelBlockyModel::BakedData &baked_model = baked_models[0];
	Ref<Mesh> mesh = VoxelBlockyModel::make_mesh_from_baked_data(baked_model, require_tangents);

	for (unsigned int surface_index = 0; surface_index < baked_model.model.surface_count; ++surface_index) {
		const unsigned int material_index = baked_model.model.surfaces[surface_index].material_id;
		Ref<Material> material = materials[material_index];
		mesh->surface_set_material(surface_index, material);
	}

	return mesh;
}

template <typename T, typename F>
void unordered_remove_duplicates(StdVector<T> &container, F equality) {
	for (unsigned int i = 0; i < container.size(); ++i) {
		const T &item1 = container[i];
		for (unsigned int j = i + 1; j < container.size();) {
			const T &item2 = container[j];
			if (equality(item1, item2)) {
				container[j] = container[container.size() - 1];
				container.pop_back();
			} else {
				++j;
			}
		}
	}
}

void VoxelBlockyType::gather_and_sort_attributes(
		const StdVector<Ref<VoxelBlockyAttribute>> &attributes_with_maybe_nulls,
		StdVector<Ref<VoxelBlockyAttribute>> &out_attributes) {
	ZN_PROFILE_SCOPE();

	// Gather non-null attributes
	for (const Ref<VoxelBlockyAttribute> &attrib : attributes_with_maybe_nulls) {
		if (attrib.is_valid()) {
			out_attributes.push_back(attrib);
			continue;
		}
	}

	unordered_remove_duplicates(
			out_attributes, [](const Ref<VoxelBlockyAttribute> &a, const Ref<VoxelBlockyAttribute> &b) {
				return a->get_attribute_name() == b->get_attribute_name();
			});

	// Sort attributes by name for determinism
	// TODO Should we just consider attribute slots rather than an unordered variable-length list of attributes? Or
	// require that the order of attributes matters? Adding a new attribute can mess up existing keys
	VoxelBlockyAttribute::sort_by_name(to_span(out_attributes));
}

void VoxelBlockyType::generate_keys(const StdVector<Ref<VoxelBlockyAttribute>> &attributes,
		StdVector<VariantKey> &out_keys, bool include_rotations) {
	ZN_PROFILE_SCOPE();

	for (unsigned int i = 0; i < attributes.size(); ++i) {
		ZN_ASSERT_RETURN(attributes[i].is_valid());
	}

	// When `include_rotations` is false, rotation attributes may be considered having a single value, their
	// default value. So we cache used values for each attribute.
	FixedArray<Span<const uint8_t>, MAX_ATTRIBUTES> attributes_used_values;
	FixedArray<uint8_t, MAX_ATTRIBUTES> attributes_default_values;
	for (unsigned int i = 0; i < attributes.size(); ++i) {
		const VoxelBlockyAttribute &attrib = **attributes[i];
		if (include_rotations == false && attrib.is_rotation()) {
			attributes_default_values[i] = attrib.get_default_value();
			attributes_used_values[i] = Span<uint8_t>(&attributes_default_values[i], 1);
		} else {
			attributes_used_values[i] = attrib.get_used_values();
		}
	}

	// Get variant count
	unsigned int variant_count = 1;
	for (unsigned int i = 0; i < attributes.size(); ++i) {
		variant_count *= attributes_used_values[i].size();
	}

	// TODO Return combination count so we can check if it matches returned key count, and we can show feedback in the
	// inspector using a dummy property as indicator.
	ZN_ASSERT_RETURN_MSG(variant_count < MAX_EDITING_VARIANTS, "Too many combinations");

	StdVector<VariantKey> &keys = out_keys;
	keys.resize(variant_count);

	// Generate combinations

	FixedArray<unsigned int, VoxelBlockyType::MAX_ATTRIBUTES> key_uv;
	fill(key_uv, 0u);

	// Initialize first key
	VoxelBlockyType::VariantKey key;
	for (unsigned int i = 0; i < attributes.size(); ++i) {
		const Ref<VoxelBlockyAttribute> &attrib = attributes[i];
		key.attribute_names[i] = attrib->get_attribute_name();
		key.attribute_values[i] = attributes_used_values[i][0];
	}

	for (unsigned int i = 0; i < keys.size(); ++i) {
		keys[i] = key;

		// Increment key
		for (unsigned int j = 0; j < attributes.size(); ++j) {
			++key_uv[j];

			Span<const uint8_t> used_values = attributes_used_values[j];

			if (key_uv[j] < used_values.size()) {
				key.attribute_values[j] = used_values[key_uv[j]];
				break;
			}

			key_uv[j] = 0;
			key.attribute_values[j] = used_values[key_uv[j]];
		}
	}
}

void VoxelBlockyType::generate_keys(StdVector<VariantKey> &out_keys, bool include_rotations) const {
	StdVector<Ref<VoxelBlockyAttribute>> attributes;
	gather_and_sort_attributes(_attributes, attributes);
	generate_keys(attributes, out_keys, include_rotations);
}

void VoxelBlockyType::_on_attribute_changed() {
	// We used to do this when attributes influenced the list of properties, not anymore
	// notify_property_list_changed();

	// We do this solely as a mean to update custom controls...
	emit_changed();
}

void VoxelBlockyType::_on_base_model_changed() {
	emit_changed();
}

TypedArray<VoxelBlockyAttribute> VoxelBlockyType::_b_get_attributes() const {
	return godot::to_typed_array(to_span(_attributes));
}

void VoxelBlockyType::_b_set_attributes(TypedArray<VoxelBlockyAttribute> attributes) {
	if (attributes.size() > MAX_ATTRIBUTES) {
		return;
	}

	// Check differences. Note, we only check changes that could happen in the editor, so only attribute types and how
	// many there are.
	bool has_changes = false;
	unsigned int found_attributes = 0;
	for (int i = 0; i < attributes.size(); ++i) {
		Ref<VoxelBlockyAttribute> attrib = attributes[i];
		if (attrib.is_null()) {
			// Null is allowed for the editor to work
			continue;
		}
		unsigned int existing_index;
		if (try_get_attribute_index_from_name(_attributes, attrib->get_attribute_name(), existing_index)) {
			++found_attributes;
			continue;
		}
		// New attribute
		has_changes = true;
	}
	if (!has_changes && get_non_null_count(_attributes) != found_attributes) {
		has_changes = true;
	}

#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		for (Ref<VoxelBlockyAttribute> &attrib : _attributes) {
			if (attrib.is_valid()) {
				attrib->disconnect(VoxelStringNames::get_singleton().changed,
						callable_mp(this, &VoxelBlockyType::_on_attribute_changed));
			}
		}
	}
#endif

	godot::copy_to(_attributes, attributes);

#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		for (Ref<VoxelBlockyAttribute> &attrib : _attributes) {
			if (attrib.is_valid()) {
				attrib->connect(VoxelStringNames::get_singleton().changed,
						callable_mp(this, &VoxelBlockyType::_on_attribute_changed));
			}
		}
	}
#endif

	if (has_changes) {
		notify_property_list_changed();
	}

	emit_changed();
}

void VoxelBlockyType::_b_set_variant_model(Array p_key, Ref<VoxelBlockyModel> model) {
	VariantKey key;
	ZN_ASSERT_RETURN(key.parse_from_array(p_key));
	set_variant(key, model);
}

void VoxelBlockyType::_b_set_variant_models_data(Array data) {
	_variants.clear();
	for (int i = 0; i < data.size(); ++i) {
		Variant pair_v = data[i];
		ZN_ASSERT_CONTINUE(pair_v.get_type() == Variant::ARRAY);
		Array pair = pair_v;
		ZN_ASSERT_CONTINUE(pair.size() == 2);
		Variant key_v = pair[0];
		Variant model_v = pair[1];
		ZN_ASSERT_CONTINUE(key_v.get_type() == Variant::ARRAY);
		VariantKey key;
		ZN_ASSERT_CONTINUE(key.parse_from_array(key_v));
		Ref<VoxelBlockyModel> model = model_v;
		if (model.is_null()) {
			continue;
		}
		VariantData vd;
		vd.key = key;
		vd.model = model;
		_variants.push_back(vd);
	}
}

Array VoxelBlockyType::_b_get_variant_models_data() const {
	ZN_PROFILE_SCOPE();
	// Instead of just saving the constants of `_variants`, we only gather valid ones, and cleanup the others.

	StdVector<Ref<VoxelBlockyAttribute>> attributes;
	gather_and_sort_attributes(_attributes, attributes);

	StdVector<VariantKey> keys;
	generate_keys(attributes, keys, !_automatic_rotations);

	Array data;
	for (const VariantKey &key : keys) {
		Ref<VoxelBlockyModel> model = get_variant(key);
		if (model.is_null()) {
			continue;
		}
		Array pair;
		pair.resize(2);
		pair[0] = key.to_array();
		pair[1] = model;
		data.append(pair);
	}

	return data;
}

void VoxelBlockyType::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_unique_name", "name"), &VoxelBlockyType::set_unique_name);
	ClassDB::bind_method(D_METHOD("get_unique_name"), &VoxelBlockyType::get_unique_name);

	ClassDB::bind_method(D_METHOD("set_base_model", "model"), &VoxelBlockyType::set_base_model);
	ClassDB::bind_method(D_METHOD("get_base_model"), &VoxelBlockyType::get_base_model);

	ClassDB::bind_method(D_METHOD("set_attributes", "attributes"), &VoxelBlockyType::_b_set_attributes);
	ClassDB::bind_method(D_METHOD("get_attributes"), &VoxelBlockyType::_b_get_attributes);

	ClassDB::bind_method(D_METHOD("get_rotation_attribute"), &VoxelBlockyType::get_rotation_attribute);

	ClassDB::bind_method(D_METHOD("set_variant_model"), &VoxelBlockyType::_b_set_variant_model);

	ClassDB::bind_method(D_METHOD("_get_variant_models_data"), &VoxelBlockyType::_b_get_variant_models_data);
	ClassDB::bind_method(D_METHOD("_set_variant_models_data", "data"), &VoxelBlockyType::_b_set_variant_models_data);

	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "unique_name"), "set_unique_name", "get_unique_name");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "base_model", PROPERTY_HINT_RESOURCE_TYPE,
						 VoxelBlockyModel::get_class_static()),
			"set_base_model", "get_base_model");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "attributes", PROPERTY_HINT_ARRAY_TYPE,
						 MAKE_RESOURCE_TYPE_HINT(VoxelBlockyAttribute::get_class_static())),
			"set_attributes", "get_attributes");

	// This property is only for saving, not for direct use by scripts or inspector.
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "_variant_models_data", PROPERTY_HINT_NONE, "",
						 PROPERTY_USAGE_STORAGE
								 // But editor is required so we can insert a custom editor instead of the property
								 | PROPERTY_USAGE_EDITOR),
			"_set_variant_models_data", "_get_variant_models_data");

	BIND_CONSTANT(MAX_ATTRIBUTES);
}

String VoxelBlockyType::VariantKey::to_string() const {
	const VoxelBlockyType::VariantKey &key = *this;
	String s;
	for (unsigned int attribute_index = 0; attribute_index < key.attribute_names.size(); ++attribute_index) {
		const StringName &attrib_name = key.attribute_names[attribute_index];
		if (attrib_name == StringName()) {
			break;
		}
		if (attribute_index > 0) {
			s += ",";
		}
		s += String(attrib_name);
		s += '=';
		s += String::num_int64(key.attribute_values[attribute_index]);
	}
	return s;
}

String VoxelBlockyType::VariantKey::to_string(Span<const Ref<VoxelBlockyAttribute>> context_attributes) const {
	const VoxelBlockyType::VariantKey &key = *this;
	String s;
	for (unsigned int attribute_index = 0; attribute_index < key.attribute_names.size(); ++attribute_index) {
		const StringName &attrib_name = key.attribute_names[attribute_index];
		if (attrib_name == StringName()) {
			break;
		}
		if (attribute_index > 0) {
			s += ",";
		}
		s += String(attrib_name);
		s += '=';
		String value_str;
		if (attribute_index < context_attributes.size()) {
			const Ref<VoxelBlockyAttribute> &attrib = context_attributes[attribute_index];
			ZN_ASSERT_RETURN_V(attrib.is_valid(), "<error>");
			if (attrib->get_attribute_name() == attrib_name) {
				value_str = attrib->get_name_from_value(key.attribute_values[attribute_index]);
			}
		}
		if (value_str.is_empty()) {
			value_str = String::num_int64(key.attribute_values[attribute_index]);
		}
		s += value_str;
	}
	return s;
}

bool VoxelBlockyType::VariantKey::parse_from_array(const Array &array) {
	ZN_ASSERT_RETURN_V((array.size() % 2) == 0, false);
	for (int i = 0; i < array.size(); i += 2) {
		Variant name_v = array[i];
		Variant value_v = array[i + 1];

		ZN_ASSERT_RETURN_V(name_v.get_type() == Variant::STRING_NAME, false);
		ZN_ASSERT_RETURN_V(value_v.get_type() == Variant::INT, false);

		// TODO GDX: Variant has no operator to convert to 8-bit integers in GodotCpp
		const int value = value_v;
		ZN_ASSERT_RETURN_V(value >= 0 && value < VoxelBlockyAttribute::MAX_VALUES, false);

		const int attrib_index = i / 2;
		attribute_names[attrib_index] = name_v;
		attribute_values[attrib_index] = value;
	}
	return true;
}

Array VoxelBlockyType::VariantKey::to_array() const {
	Array array;
	for (unsigned int i = 0; i < attribute_names.size(); ++i) {
		const StringName &attribute_name = attribute_names[i];
		if (attribute_name == StringName()) {
			break;
		}
		array.append(attribute_name);
		array.append(attribute_values[i]);
	}
	return array;
}

} // namespace zylann::voxel
