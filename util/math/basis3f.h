#ifndef ZN_MATH_BASIS3F_H
#define ZN_MATH_BASIS3F_H

#include "vector3f.h"

namespace zylann::math {

struct Basis3f {
	Vector3f rows[3] = { //
		Vector3f(1, 0, 0), //
		Vector3f(0, 1, 0), //
		Vector3f(0, 0, 1)
	};

	void set_axis_angle(const Vector3f p_axis, float cosine, float sine) {
		// Rotation matrix from axis and angle, see
		// https://en.wikipedia.org/wiki/Rotation_matrix#Rotation_matrix_from_axis_angle
		// #ifdef DEBUG_ENABLED
		// 		ZN_ASSERT_RETURN(math::is_normalized(p_axis));
		// #endif
		const Vector3f axis_sq(p_axis.x * p_axis.x, p_axis.y * p_axis.y, p_axis.z * p_axis.z);
		rows[0][0] = axis_sq.x + cosine * (1.0f - axis_sq.x);
		rows[1][1] = axis_sq.y + cosine * (1.0f - axis_sq.y);
		rows[2][2] = axis_sq.z + cosine * (1.0f - axis_sq.z);

		const float t = 1 - cosine;

		float xyzt = p_axis.x * p_axis.y * t;
		float zyxs = p_axis.z * sine;
		rows[0][1] = xyzt - zyxs;
		rows[1][0] = xyzt + zyxs;

		xyzt = p_axis.x * p_axis.z * t;
		zyxs = p_axis.y * sine;
		rows[0][2] = xyzt + zyxs;
		rows[2][0] = xyzt - zyxs;

		xyzt = p_axis.y * p_axis.z * t;
		zyxs = p_axis.x * sine;
		rows[1][2] = xyzt - zyxs;
		rows[2][1] = xyzt + zyxs;
	}

	inline Vector3f xform(const Vector3f v) const {
		return Vector3f( //
				math::dot(rows[0], v), //
				math::dot(rows[1], v), //
				math::dot(rows[2], v) //
		);
	}
};

inline Vector3f rotated(const Vector3f v, const Vector3f axis, float cosine, float sine) {
	Basis3f b;
	b.set_axis_angle(axis, cosine, sine);
	return b.xform(v);
}

} // namespace zylann::math

#endif // ZN_MATH_BASIS3F_H
