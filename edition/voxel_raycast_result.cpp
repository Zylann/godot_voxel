#include "voxel_raycast_result.h"

Vector3 VoxelRaycastResult::_b_get_position() const {
	return position.to_vec3();
}

Vector3 VoxelRaycastResult::_b_get_previous_position() const {
	return previous_position.to_vec3();
}

float VoxelRaycastResult::_b_get_distance() const {
	return distance_along_ray;
}

void VoxelRaycastResult::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_position"), &VoxelRaycastResult::_b_get_position);
	ClassDB::bind_method(D_METHOD("get_previous_position"), &VoxelRaycastResult::_b_get_previous_position);
	ClassDB::bind_method(D_METHOD("get_distance"), &VoxelRaycastResult::_b_get_distance);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "position"), "", "get_position");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "previous_position"), "", "get_previous_position");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "distance"), "", "get_distance");
}
