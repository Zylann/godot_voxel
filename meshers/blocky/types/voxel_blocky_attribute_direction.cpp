#include "voxel_blocky_attribute_direction.h"
#include "../../../constants/voxel_string_names.h"
#include "../../../util/math/ortho_basis.h"

namespace zylann::voxel {

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

	bases[VoxelBlockyAttributeDirection::DIR_NEGATIVE_X].rotate_y_90_ccw();

	bases[VoxelBlockyAttributeDirection::DIR_POSITIVE_X].rotate_y_90_cw();

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

} // namespace zylann::voxel
