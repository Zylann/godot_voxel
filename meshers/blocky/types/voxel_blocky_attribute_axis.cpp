#include "voxel_blocky_attribute_axis.h"
#include "../../../constants/voxel_string_names.h"
#include "../../../util/math/ortho_basis.h"

namespace zylann::voxel {

VoxelBlockyAttributeAxis::VoxelBlockyAttributeAxis() {
	_name = VoxelStringNames::get_singleton().axis;
	_is_rotation = true;
	_default_value = AXIS_Z;

	_value_names.resize(AXIS_COUNT);
	_value_names[AXIS_X] = VoxelStringNames::get_singleton().x;
	_value_names[AXIS_Y] = VoxelStringNames::get_singleton().y;
	_value_names[AXIS_Z] = VoxelStringNames::get_singleton().z;

	FixedArray<math::OrthoBasis, VoxelBlockyAttributeAxis::AXIS_COUNT> bases;
	bases[VoxelBlockyAttributeAxis::AXIS_X].rotate_y_90_ccw();
	bases[VoxelBlockyAttributeAxis::AXIS_Y].rotate_x_90_ccw();
	// AXIS_Z is identity

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

} // namespace zylann::voxel
