#include "vector3i.h"
#include <sstream>

namespace zylann {

std::stringstream &operator<<(std::stringstream &ss, const Vector3i &v) {
	ss << "(" << v.x << ", " << v.y << ", " << v.z << ")";
	return ss;
}

namespace math {

Vector3i rotate_90(Vector3i v, Vector3i::Axis axis, bool clockwise) {
	if (axis == Vector3::AXIS_X) {
		if (clockwise) {
			return math::rotate_x_90_cw(v);
		} else {
			return math::rotate_x_90_ccw(v);
		}
	} else if (axis == Vector3::AXIS_Y) {
		if (clockwise) {
			return math::rotate_y_90_cw(v);
		} else {
			return math::rotate_y_90_ccw(v);
		}
	} else if (axis == Vector3::AXIS_Z) {
		if (clockwise) {
			return math::rotate_z_90_cw(v);
		} else {
			return math::rotate_z_90_ccw(v);
		}
	} else {
		ZN_PRINT_ERROR("Invalid axis");
		return v;
	}
}

} // namespace math
} // namespace zylann
