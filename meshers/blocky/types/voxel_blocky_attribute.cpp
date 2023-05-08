#include "voxel_blocky_attribute.h"
#include "../../../constants/voxel_string_names.h"
#include "../../../util/container_funcs.h"
#include "../../../util/log.h"
#include "../../../util/math/funcs.h"
#include "../../../util/math/ortho_basis.h"

namespace zylann::voxel {

StringName VoxelBlockyAttribute::get_attribute_name() const {
	return _name;
}

int VoxelBlockyAttribute::get_value_count() const {
	return _value_names.size();
}

int VoxelBlockyAttribute::get_default_value() const {
	return _default_value;
}

void VoxelBlockyAttribute::set_default_value(int value) {
	// TODO Can't validate that the value is in bounds, because we can't guarantee the order in which Godot will call
	// setters of the resource when loading it
	_default_value = math::max(value, 0);
}

int VoxelBlockyAttribute::get_value_from_name(StringName name) const {
	for (int i = 0; i < _value_names.size(); ++i) {
		if (_value_names[i] == name) {
			return i;
		}
	}
	return -1;
}

StringName VoxelBlockyAttribute::get_name_from_value(int v) const {
	ZN_ASSERT_RETURN_V(v >= 0 && v < get_value_count(), StringName());
	return _value_names[v];
}

Span<const uint8_t> VoxelBlockyAttribute::get_used_values() const {
	return to_span(_used_values);
}

bool VoxelBlockyAttribute::is_value_used(int v) const {
	for (const uint8_t used_value : _used_values) {
		if (v == used_value) {
			return true;
		}
	}
	return false;
}

bool VoxelBlockyAttribute::is_equivalent(const VoxelBlockyAttribute &other) const {
	if (_name != other._name) {
		return false;
	}
	Span<const uint8_t> self_used_values = get_used_values();
	Span<const uint8_t> other_used_values = other.get_used_values();
	if (self_used_values.size() != other_used_values.size()) {
		return false;
	}
	for (unsigned int i = 0; i < self_used_values.size(); ++i) {
		if (self_used_values[i] != other_used_values[i]) {
			return false;
		}
	}
	for (unsigned int i = 0; i < self_used_values.size(); ++i) {
		const String value_name = get_name_from_value(self_used_values[i]);
		const String other_value_name = other.get_name_from_value(other_used_values[i]);
		if (value_name != other_value_name) {
			return false;
		}
	}
	return true;
}

bool find_non_empty_duplicate(const std::vector<StringName> &names) {
	for (unsigned int i = 0; i < names.size(); ++i) {
		const StringName &a = names[i];
		if (a == StringName()) {
			continue;
		}
		for (unsigned int j = i + 1; j < names.size(); ++j) {
			const StringName &b = names[j];
			if (b == StringName()) {
				continue;
			}
			if (a == b) {
				return true;
			}
		}
	}
	return false;
}

void VoxelBlockyAttribute::get_configuration_warnings(PackedStringArray &out_warnings) const {
	if (find_non_empty_duplicate(_value_names)) {
		out_warnings.push_back(String("{0} named {1} has multiple values with the same name.")
									   .format(varray(get_class(), get_attribute_name())));
	}

	if (!contains(to_span(_used_values), _default_value)) {
		out_warnings.push_back(String("{0} named {1} has an invalid default value.")
									   .format(varray(get_class(), get_attribute_name())));
	}
}

// int VoxelBlockyAttribute::get_order() const {
// 	ZN_PRINT_ERROR("Not implemented");
// 	// Implemented in child classes
// 	return 0;
// }

unsigned int VoxelBlockyAttribute::get_ortho_rotation_index_from_value(int value) const {
	ZN_ASSERT_RETURN_V(value >= 0 && value < _value_names.size(), math::ORTHO_ROTATION_IDENTITY);
	if (value >= int(_ortho_rotations.size())) {
		return math::ORTHO_ROTATION_IDENTITY;
	}
	return _ortho_rotations[value];
}

void VoxelBlockyAttribute::sort_by_name(Span<Ref<VoxelBlockyAttribute>> attributes) {
	for (const Ref<VoxelBlockyAttribute> &attrib : attributes) {
		ZN_ASSERT_RETURN(attrib.is_valid());
	}
	struct AttributeComparator {
		bool operator()(const Ref<VoxelBlockyAttribute> &a, const Ref<VoxelBlockyAttribute> &b) const {
			return String(a->get_attribute_name()) < String(b->get_attribute_name());
		}
	};
	SortArray<Ref<VoxelBlockyAttribute>, AttributeComparator> sorter;
	sorter.sort(attributes.data(), attributes.size());
}

void VoxelBlockyAttribute::sort_by_name(Span<StringName> attributes) {
	for (const StringName &attrib : attributes) {
		ZN_ASSERT_RETURN(attrib != StringName());
	}
	struct AttributeComparator {
		bool operator()(const StringName &a, const StringName &b) const {
			return String(a) < String(b);
		}
	};
	SortArray<StringName, AttributeComparator> sorter;
	sorter.sort(attributes.data(), attributes.size());
}

void VoxelBlockyAttribute::sort_by_name(Span<std::pair<StringName, uint8_t>> attributes) {
	for (const std::pair<StringName, uint8_t> &attrib : attributes) {
		ZN_ASSERT_RETURN(attrib.first != StringName());
	}
	struct AttributeComparator {
		bool operator()(const std::pair<StringName, uint8_t> &a, const std::pair<StringName, uint8_t> &b) const {
			return String(a.first) < String(b.first);
		}
	};
	SortArray<std::pair<StringName, uint8_t>, AttributeComparator> sorter;
	sorter.sort(attributes.data(), attributes.size());
}

void VoxelBlockyAttribute::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_attribute_name"), &VoxelBlockyAttributeCustom::get_attribute_name);
	ClassDB::bind_method(D_METHOD("get_default_value"), &VoxelBlockyAttributeCustom::get_default_value);
	ClassDB::bind_method(D_METHOD("get_value_count"), &VoxelBlockyAttributeCustom::get_value_count);
	ClassDB::bind_method(D_METHOD("is_rotation"), &VoxelBlockyAttributeCustom::is_rotation);

	BIND_CONSTANT(MAX_VALUES);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

VoxelBlockyAttributeAxis::VoxelBlockyAttributeAxis() {
	_name = VoxelStringNames::get_singleton().axis;
	_is_rotation = true;

	_value_names.resize(AXIS_COUNT);
	_value_names[AXIS_X] = VoxelStringNames::get_singleton().x;
	_value_names[AXIS_Y] = VoxelStringNames::get_singleton().y;
	_value_names[AXIS_Z] = VoxelStringNames::get_singleton().z;

	FixedArray<math::OrthoBasis, VoxelBlockyAttributeAxis::AXIS_COUNT> bases;
	// AXIS_X is identity
	bases[VoxelBlockyAttributeAxis::AXIS_Y].rotate_x_90_ccw();
	bases[VoxelBlockyAttributeAxis::AXIS_Z].rotate_y_90_ccw();

	_ortho_rotations.resize(bases.size());
	for (unsigned int axis_index = 0; axis_index < bases.size(); ++axis_index) {
		_ortho_rotations[axis_index] = math::get_index_from_ortho_basis(bases[axis_index]);
	}

	update_values();
}

void VoxelBlockyAttributeAxis::set_horizontal_only(bool h) {
	if (h != _horizontal_only) {
		_horizontal_only = h;
		update_values();
		emit_changed();
	}
}

bool VoxelBlockyAttributeAxis::is_horizontal_only() const {
	return _horizontal_only;
}

void VoxelBlockyAttributeAxis::update_values() {
	_used_values.clear();
	_used_values.reserve(AXIS_COUNT);
	_used_values.push_back(AXIS_X);
	if (_horizontal_only == false) {
		_used_values.push_back(AXIS_Y);
	}
	_used_values.push_back(AXIS_Z);
}

int VoxelBlockyAttributeAxis::from_vec3(Vector3 v) const {
	if (_horizontal_only) {
		v.y = 0;
	}
	return math::get_longest_axis(to_vec3f(v));
}

void VoxelBlockyAttributeAxis::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_horizontal_only"), &VoxelBlockyAttributeAxis::is_horizontal_only);
	ClassDB::bind_method(D_METHOD("set_horizontal_only", "enabled"), &VoxelBlockyAttributeAxis::set_horizontal_only);
	ClassDB::bind_method(D_METHOD("from_vec3", "v"), &VoxelBlockyAttributeAxis::from_vec3);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "horizontal_only"), "set_horizontal_only", "is_horizontal_only");

	BIND_ENUM_CONSTANT(AXIS_X);
	BIND_ENUM_CONSTANT(AXIS_Y);
	BIND_ENUM_CONSTANT(AXIS_Z);
	BIND_ENUM_CONSTANT(AXIS_COUNT);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

VoxelBlockyAttributeDirection::VoxelBlockyAttributeDirection() {
	_name = VoxelStringNames::get_singleton().direction;
	_is_rotation = true;
	_default_value = DIR_NEGATIVE_Z; // Forward

	_value_names.resize(DIR_COUNT);
	_value_names[DIR_NEGATIVE_X] = VoxelStringNames::get_singleton().negative_x;
	_value_names[DIR_POSITIVE_X] = VoxelStringNames::get_singleton().positive_x;
	_value_names[DIR_NEGATIVE_Y] = VoxelStringNames::get_singleton().negative_y;
	_value_names[DIR_POSITIVE_Y] = VoxelStringNames::get_singleton().positive_y;
	_value_names[DIR_NEGATIVE_Z] = VoxelStringNames::get_singleton().negative_z;
	_value_names[DIR_POSITIVE_Z] = VoxelStringNames::get_singleton().positive_z;

	FixedArray<math::OrthoBasis, VoxelBlockyAttributeDirection::DIR_COUNT> bases;

	// bases[VoxelBlockyAttributeDirection::DIR_NEGATIVE_Z]; // Identity

	bases[VoxelBlockyAttributeDirection::DIR_POSITIVE_Z].rotate_y_90_ccw();
	bases[VoxelBlockyAttributeDirection::DIR_POSITIVE_Z].rotate_y_90_ccw();

	bases[VoxelBlockyAttributeDirection::DIR_NEGATIVE_X].rotate_y_90_cw();

	bases[VoxelBlockyAttributeDirection::DIR_POSITIVE_X].rotate_y_90_ccw();

	bases[VoxelBlockyAttributeDirection::DIR_NEGATIVE_Y].rotate_x_90_cw();

	bases[VoxelBlockyAttributeDirection::DIR_POSITIVE_Y].rotate_x_90_ccw();

	_ortho_rotations.resize(bases.size());
	for (unsigned int dir_index = 0; dir_index < bases.size(); ++dir_index) {
		_ortho_rotations[dir_index] = math::get_index_from_ortho_basis(bases[dir_index]);
	}

	update_values();
}

void VoxelBlockyAttributeDirection::set_horizontal_only(bool h) {
	if (h != _horizontal_only) {
		_horizontal_only = h;
		update_values();
		emit_changed();
	}
}

void VoxelBlockyAttributeDirection::update_values() {
	_used_values.clear();
	_used_values.reserve(DIR_COUNT);
	_used_values.push_back(DIR_NEGATIVE_X);
	_used_values.push_back(DIR_POSITIVE_X);
	if (_horizontal_only == false) {
		_used_values.push_back(DIR_NEGATIVE_Y);
		_used_values.push_back(DIR_POSITIVE_Y);
	}
	_used_values.push_back(DIR_NEGATIVE_Z);
	_used_values.push_back(DIR_POSITIVE_Z);
}

int VoxelBlockyAttributeDirection::from_vec3(Vector3 v) const {
	if (_horizontal_only) {
		v.y = 0;
	}
	const int longest_axis = math::get_longest_axis(to_vec3f(v));
	switch (longest_axis) {
		case Vector3::AXIS_X:
			return v.x < 0 ? DIR_NEGATIVE_X : DIR_POSITIVE_X;
		case Vector3::AXIS_Y:
			return v.y < 0 ? DIR_NEGATIVE_Y : DIR_POSITIVE_Y;
		case Vector3::AXIS_Z:
			return v.z < 0 ? DIR_NEGATIVE_Z : DIR_POSITIVE_Z;
		default:
			ZN_PRINT_ERROR("Unexpected axis");
			return -1;
	}
}

bool VoxelBlockyAttributeDirection::is_horizontal_only() const {
	return _horizontal_only;
}

void VoxelBlockyAttributeDirection::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_horizontal_only"), &VoxelBlockyAttributeDirection::is_horizontal_only);
	ClassDB::bind_method(
			D_METHOD("set_horizontal_only", "enabled"), &VoxelBlockyAttributeDirection::set_horizontal_only);
	ClassDB::bind_method(D_METHOD("from_vec3", "v"), &VoxelBlockyAttributeDirection::from_vec3);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "horizontal_only"), "set_horizontal_only", "is_horizontal_only");

	BIND_ENUM_CONSTANT(DIR_NEGATIVE_X);
	BIND_ENUM_CONSTANT(DIR_POSITIVE_X);
	BIND_ENUM_CONSTANT(DIR_NEGATIVE_Y);
	BIND_ENUM_CONSTANT(DIR_POSITIVE_Y);
	BIND_ENUM_CONSTANT(DIR_NEGATIVE_Z);
	BIND_ENUM_CONSTANT(DIR_POSITIVE_Z);
	BIND_ENUM_CONSTANT(DIR_COUNT);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Full-on orthogonal rotation is actually hard to use directly as a single property... there cannot be straightforward
// unique names for all 24 values, as well as the raw integer values. It may always need an intermediary tool or helper
// function to get/set rotations, usually starting from an existing state and rotating around one or two axes.

VoxelBlockyAttributeRotation::VoxelBlockyAttributeRotation() {
	_name = VoxelStringNames::get_singleton().rotation;
	_is_rotation = true;

	_value_names.resize(math::ORTHOGONAL_BASIS_COUNT);
	for (unsigned int i = 0; i < _value_names.size(); ++i) {
		_value_names[i] = VoxelStringNames::get_singleton().ortho_rotation_names[i];
	}

	update_values();

	_ortho_rotations.resize(math::ORTHOGONAL_BASIS_COUNT);
	for (unsigned int i = 0; i < _ortho_rotations.size(); ++i) {
		_ortho_rotations[i] = i;
	}
}

void VoxelBlockyAttributeRotation::set_horizontal_roll_enabled(bool enable) {
	if (enable != _horizontal_roll_enabled) {
		_horizontal_roll_enabled = enable;
		update_values();
		emit_changed();
	}
}

bool VoxelBlockyAttributeRotation::is_horizontal_roll_enabled() const {
	return _horizontal_roll_enabled;
}

void VoxelBlockyAttributeRotation::update_values() {
	_used_values.clear();
	_used_values.reserve(math::ORTHOGONAL_BASIS_COUNT);
	if (_horizontal_roll_enabled) {
		for (unsigned int ortho_index = 0; ortho_index < math::ORTHOGONAL_BASIS_COUNT; ++ortho_index) {
			_used_values.push_back(ortho_index);
		}
	} else {
		for (unsigned int ortho_index = 0; ortho_index < math::ORTHOGONAL_BASIS_COUNT; ++ortho_index) {
			const math::OrthoBasis &basis = math::get_ortho_basis_from_index(ortho_index);
			if (basis.y != math::Vector3i8(0, 1, 0) && basis.z.y == 0) {
				// Skip rotations where Y is not up and Z is not horizontal
				continue;
			}
			_used_values.push_back(ortho_index);
		}
	}
}

void VoxelBlockyAttributeRotation::_bind_methods() {
	ClassDB::bind_method(
			D_METHOD("is_horizontal_roll_enabled"), &VoxelBlockyAttributeRotation::is_horizontal_roll_enabled);
	ClassDB::bind_method(D_METHOD("set_horizontal_roll_enabled", "enabled"),
			&VoxelBlockyAttributeRotation::set_horizontal_roll_enabled);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "horizontal_only"), "set_horizontal_roll_enabled",
			"is_horizontal_roll_enabled");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

VoxelBlockyAttributeCustom::VoxelBlockyAttributeCustom() {
	// Defaults to a boolean value.
	_value_names.resize(2);
	update_values();
}

void VoxelBlockyAttributeCustom::set_attribute_name(StringName name) {
	if (_name != name) {
		_name = name;
		emit_changed();
	}
}

void VoxelBlockyAttributeCustom::set_value_count(int count) {
	count = math::clamp(count, 2, MAX_VALUES);
	if (count != _value_names.size()) {
		_value_names.resize(count);
		update_values();
		notify_property_list_changed();
		emit_changed();
		// Can't check validity of default value because when loading the resource Godot can set it in any order...
	}
}

void VoxelBlockyAttributeCustom::set_value_name(int index, StringName name) {
	_value_names[index] = name;
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
	ADD_PROPERTY(PropertyInfo(Variant::INT, "is_rotation"), "set_is_rotation", "is_rotation");
}

} // namespace zylann::voxel
