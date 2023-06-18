#include "voxel_blocky_attribute_custom.h"
#include "../../../util/math/funcs.h"
#include "../../../util/math/ortho_basis.h"

namespace zylann::voxel {

VoxelBlockyAttributeCustom::VoxelBlockyAttributeCustom() {
	// Defaults to a boolean value.
	_value_names.resize(2);
	update_values();
}

void VoxelBlockyAttributeCustom::set_attribute_name(StringName p_name) {
	if (_name != p_name) {
		_name = p_name;
		emit_changed();
	}
}

void VoxelBlockyAttributeCustom::set_value_count(int count) {
	ERR_FAIL_COND(count < 0);
	count = math::clamp(count, 2, MAX_VALUES);
	if (count != int(_value_names.size())) {
		_value_names.resize(count);
		update_values();
		notify_property_list_changed();
		emit_changed();
		// Can't check validity of default value because when loading the resource Godot can set it in any order...
	}
}

void VoxelBlockyAttributeCustom::set_value_name(int index, StringName p_name) {
	_value_names[index] = p_name;
}

void VoxelBlockyAttributeCustom::set_default_value(int v) {
	v = math::clamp(v, 0, MAX_VALUES);
	// Can't check validity of default value because when loading the resource Godot can set it in any order...
	if (v != _default_value) {
		_default_value = v;
		emit_changed();
	}
}

// void VoxelBlockyAttributeCustom::set_is_rotation(bool is_rotation) {
// 	if (is_rotation != _is_rotation) {
// 		_is_rotation = is_rotation;
// 		notify_property_list_changed();
// 		emit_changed();
// 	}
// }

// void VoxelBlockyAttributeCustom::set_value_ortho_rotation(int index, int ortho_rotation_index) {
// 	ZN_ASSERT_RETURN(index >= 0 && index < int(get_value_count()));
// 	ZN_ASSERT_RETURN(ortho_rotation_index >= 0 && ortho_rotation_index < math::ORTHO_ROTATION_COUNT);
// 	_ortho_rotations[index] = ortho_rotation_index;
// 	emit_changed();
// }

void VoxelBlockyAttributeCustom::update_values() {
	_used_values.resize(_value_names.size());
	for (unsigned int i = 0; i < _used_values.size(); ++i) {
		_used_values[i] = i;
	}
	_ortho_rotations.resize(_value_names.size(), math::ORTHO_ROTATION_IDENTITY);
}

bool VoxelBlockyAttributeCustom::_set(const StringName &p_name, const Variant &p_value) {
	String name_str(p_name);

	if (name_str.begins_with("values/")) {
		int idx = name_str.get_slicec('/', 1).to_int();
		String what = name_str.get_slicec('/', 2);

		if (what == "name") {
			// Godot can set properties in any order so we have to be permissive here...
			ZN_ASSERT_RETURN_V(idx >= 0 && idx < MAX_VALUES, false);
			if (idx >= int(_value_names.size())) {
				_value_names.resize(idx + 1);
			}
			_value_names[idx] = p_value;
			return true;
		}
		// else if (what == "rotation") {
		// 	ZN_ASSERT_RETURN_V(idx >= 0 && idx < MAX_VALUES, false);
		// 	if (idx >= int(_ortho_rotations.size())) {
		// 		_ortho_rotations.resize(idx + 1);
		// 	}
		// 	_ortho_rotations[idx] = p_value;
		// 	return true;
		// }
	}

	return false;
}

bool VoxelBlockyAttributeCustom::_get(const StringName &p_name, Variant &r_ret) const {
	String name_str(p_name);

	if (name_str.begins_with("values/")) {
		int idx = name_str.get_slicec('/', 1).to_int();
		String what = name_str.get_slicec('/', 2);

		if (what == "name") {
			r_ret = get_name_from_value(idx);
			return true;
		}
		// else if (what == "rotation") {
		// 	r_ret = get_ortho_rotation_index_from_value(idx);
		// 	return true;
		// }
	}

	return false;
}

void VoxelBlockyAttributeCustom::_get_property_list(List<PropertyInfo> *p_list) const {
	for (unsigned int i = 0; i < _value_names.size(); ++i) {
		String property_base_name = "values/";
		property_base_name += String::num_int64(i);
		property_base_name += "/";

		String name_property_name = property_base_name + "name";
		p_list->push_back(PropertyInfo(Variant::STRING_NAME, name_property_name));

		// if (_is_rotation) {
		// 	String rotation_property_name = property_base_name + "rotation";
		// 	p_list->push_back(PropertyInfo(Variant::INT, rotation_property_name, PROPERTY_HINT_ENUM,
		// 			VoxelStringNames::get_singleton().ortho_rotation_enum_hint_string));
		// }
	}
}

void VoxelBlockyAttributeCustom::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_attribute_name", "name"), &VoxelBlockyAttributeCustom::set_attribute_name);
	ClassDB::bind_method(D_METHOD("set_value_count", "count"), &VoxelBlockyAttributeCustom::set_value_count);
	ClassDB::bind_method(
			D_METHOD("set_value_name", "value", "value_name"), &VoxelBlockyAttributeCustom::set_value_name);
	ClassDB::bind_method(D_METHOD("set_default_value", "value"), &VoxelBlockyAttributeCustom::set_default_value);
	// ClassDB::bind_method(D_METHOD("set_is_rotation", "is_rotation"), &VoxelBlockyAttributeCustom::set_is_rotation);
	// ClassDB::bind_method(D_METHOD("set_value_ortho_rotation", "ortho_rotation_index"),
	// 		&VoxelBlockyAttributeCustom::set_value_ortho_rotation);

	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "attribute_name"), "set_attribute_name", "get_attribute_name");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "value_count"), "set_value_count", "get_value_count");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "default_value"), "set_default_value", "get_default_value");
	// ADD_PROPERTY(PropertyInfo(Variant::INT, "is_rotation"), "set_is_rotation", "is_rotation");
}

} // namespace zylann::voxel
