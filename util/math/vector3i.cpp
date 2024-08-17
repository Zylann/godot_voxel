#include "vector3i.h"
#include <sstream>

namespace zylann {

StdStringStream &operator<<(StdStringStream &ss, const Vector3i &v) {
	ss << "(" << v.x << ", " << v.y << ", " << v.z << ")";
	return ss;
}

namespace math {

Vector3i rotate_90(Vector3i v, Axis axis, bool clockwise) {
	if (axis == AXIS_X) {
		if (clockwise) {
			return math::rotate_x_90_cw(v);
		} else {
			return math::rotate_x_90_ccw(v);
		}
	} else if (axis == AXIS_Y) {
		if (clockwise) {
			return math::rotate_y_90_cw(v);
		} else {
			return math::rotate_y_90_ccw(v);
		}
	} else if (axis == AXIS_Z) {
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
