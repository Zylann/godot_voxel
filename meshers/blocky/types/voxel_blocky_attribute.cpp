#include "voxel_blocky_attribute.h"
#include "../../../constants/voxel_string_names.h"
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
	ZN_ASSERT_RETURN_V(v < 0 || v > get_value_count(), StringName());
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

// int VoxelBlockyAttribute::get_order() const {
// 	ZN_PRINT_ERROR("Not implemented");
// 	// Implemented in child classes
// 	return 0;
// }

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

void VoxelBlockyAttribute::_bind_methods() {}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

VoxelBlockyAttributeAxis::VoxelBlockyAttributeAxis() {
	_name = VoxelStringNames::get_singleton().axis;
	_is_rotation = true;
	_value_names.resize(AXIS_COUNT);
	_value_names[AXIS_X] = VoxelStringNames::get_singleton().x;
	_value_names[AXIS_Y] = VoxelStringNames::get_singleton().y;
	_value_names[AXIS_Z] = VoxelStringNames::get_singleton().z;
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

void VoxelBlockyAttributeAxis::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_horizontal_only"), &VoxelBlockyAttributeAxis::is_horizontal_only);
	ClassDB::bind_method(D_METHOD("set_horizontal_only", "enabled"), &VoxelBlockyAttributeAxis::set_horizontal_only);

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

bool VoxelBlockyAttributeDirection::is_horizontal_only() const {
	return _horizontal_only;
}

void VoxelBlockyAttributeDirection::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_horizontal_only"), &VoxelBlockyAttributeDirection::is_horizontal_only);
	ClassDB::bind_method(
			D_METHOD("set_horizontal_only", "enabled"), &VoxelBlockyAttributeDirection::set_horizontal_only);

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
	// Rotations have no name for now...
	_value_names.resize(math::ORTHOGONAL_BASIS_COUNT);
	update_values();
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

} // namespace zylann::voxel
