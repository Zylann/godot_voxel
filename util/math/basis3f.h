#ifndef ZN_MATH_BASIS3F_H
#define ZN_MATH_BASIS3F_H

#include "quaternionf.h"
#include "vector3f.h"

namespace zylann {

// 3x3 matrix specialized at representing a 3D basis.
// Ported from Godot Engine using always 32-bit floats.
struct Basis3f {
	Vector3f rows[3] = { //
		Vector3f(1, 0, 0), //
		Vector3f(0, 1, 0), //
		Vector3f(0, 0, 1)
	};

	inline Basis3f() {}

	inline Basis3f(const Vector3f &p_x_axis, const Vector3f &p_y_axis, const Vector3f &p_z_axis) {
		set_column(0, p_x_axis);
		set_column(1, p_y_axis);
		set_column(2, p_z_axis);
	}

	inline Basis3f(const Quaternionf &q) {
		set_quaternion(q);
	}

	void set_quaternion(const Quaternionf &p_quaternion) {
		const float d = math::length_squared(p_quaternion);
		const float s = 2.0f / d;
		const float xs = p_quaternion.x * s, ys = p_quaternion.y * s, zs = p_quaternion.z * s;
		const float wx = p_quaternion.w * xs, wy = p_quaternion.w * ys, wz = p_quaternion.w * zs;
		const float xx = p_quaternion.x * xs, xy = p_quaternion.x * ys, xz = p_quaternion.x * zs;
		const float yy = p_quaternion.y * ys, yz = p_quaternion.y * zs, zz = p_quaternion.z * zs;

		rows[0][0] = 1.0f - (yy + zz);
		rows[0][1] = xy - wz;
		rows[0][2] = xz + wy;

		rows[1][0] = xy + wz;
		rows[1][1] = 1.0f - (xx + zz);
		rows[1][2] = yz - wx;

		rows[2][0] = xz - wy;
		rows[2][1] = yz + wx;
		rows[2][2] = 1.0f - (xx + yy);
	}

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

	inline void set_column(unsigned int p_index, const Vector3f &p_value) {
#ifdef DEBUG_ENABLED
		ZN_ASSERT(p_index < 3);
#endif
		// Set actual basis axis column (we store transposed as rows to match Godot's storage).
		rows[0][p_index] = p_value.x;
		rows[1][p_index] = p_value.y;
		rows[2][p_index] = p_value.z;
	}

	inline Vector3f get_column(unsigned int p_index) const {
#ifdef DEBUG_ENABLED
		ZN_ASSERT(p_index < 3);
#endif
		return Vector3f(rows[0][p_index], rows[1][p_index], rows[2][p_index]);
	}

	inline Vector3f xform(const Vector3f v) const {
		return Vector3f( //
				math::dot(rows[0], v), //
				math::dot(rows[1], v), //
				math::dot(rows[2], v) //
		);
	}

	void scale(float s) {
		rows[0][0] *= s;
		rows[0][1] *= s;
		rows[0][2] *= s;
		rows[1][0] *= s;
		rows[1][1] *= s;
		rows[1][2] *= s;
		rows[2][0] *= s;
		rows[2][1] *= s;
		rows[2][2] *= s;
	}

	Basis3f scaled(float s) const {
		Basis3f b = *this;
		b.scale(s);
		return b;
	}

	Vector3f get_scale_abs() const {
		return Vector3f( //
				math::length(Vector3f(rows[0][0], rows[1][0], rows[2][0])),
				math::length(Vector3f(rows[0][1], rows[1][1], rows[2][1])),
				math::length(Vector3f(rows[0][2], rows[1][2], rows[2][2])));
	}

	void orthonormalize() {
		// Gram-Schmidt Process

		Vector3f x = get_column(0);
		Vector3f y = get_column(1);
		Vector3f z = get_column(2);

		x = math::normalized(x);
		y = (y - x * (math::dot(x, y)));
		y = math::normalized(y);
		z = (z - x * (math::dot(x, z)) - y * (math::dot(y, z)));
		z = math::normalized(z);

		set_column(0, x);
		set_column(1, y);
		set_column(2, z);
	}

	Basis3f orthonormalized() const {
		Basis3f b = *this;
		b.orthonormalize();
		return b;
	}

	inline float determinant() const {
		return rows[0][0] * (rows[1][1] * rows[2][2] - rows[2][1] * rows[1][2]) -
				rows[1][0] * (rows[0][1] * rows[2][2] - rows[2][1] * rows[0][2]) +
				rows[2][0] * (rows[0][1] * rows[1][2] - rows[1][1] * rows[0][2]);
	}

	Quaternionf get_rotation_quaternion() const {
		// Assumes that the matrix can be decomposed into a proper rotation and scaling matrix as M = R.S,
		// and returns the Euler angles corresponding to the rotation part, complementing get_scale().
		// See the comment in get_scale() for further information.
		Basis3f m = orthonormalized();
		const float det = m.determinant();
		if (det < 0) {
			// Ensure that the determinant is 1, such that result is a proper rotation matrix which can be represented
			// by Euler angles.
			m.scale(-1);
		}

		return m.get_quaternion();
	}

	Quaternionf get_quaternion() const {
		//#ifdef MATH_CHECKS
		// 	ERR_FAIL_COND_V_MSG(!is_rotation(), Quaternion(), "Basis must be normalized in order to be casted to a
		// Quaternion. Use get_rotation_quaternion() or call orthonormalized() if the Basis contains linearly
		// independent vectors.");
		//#endif

		// Allow getting a quaternion from an unnormalized transform

		const Basis3f m = *this;
		const float trace = m.rows[0][0] + m.rows[1][1] + m.rows[2][2];
		float temp[4];

		if (trace > 0.0f) {
			float s = Math::sqrt(trace + 1.0f);
			temp[3] = (s * 0.5f);
			s = 0.5f / s;

			temp[0] = ((m.rows[2][1] - m.rows[1][2]) * s);
			temp[1] = ((m.rows[0][2] - m.rows[2][0]) * s);
			temp[2] = ((m.rows[1][0] - m.rows[0][1]) * s);
		} else {
			const int i = m.rows[0][0] < m.rows[1][1] ? (m.rows[1][1] < m.rows[2][2] ? 2 : 1)
													  : (m.rows[0][0] < m.rows[2][2] ? 2 : 0);
			const int j = (i + 1) % 3;
			const int k = (i + 2) % 3;

			float s = Math::sqrt(m.rows[i][i] - m.rows[j][j] - m.rows[k][k] + 1.0f);
			temp[i] = s * 0.5f;
			s = 0.5f / s;

			temp[3] = (m.rows[k][j] - m.rows[j][k]) * s;
			temp[j] = (m.rows[j][i] + m.rows[i][j]) * s;
			temp[k] = (m.rows[k][i] + m.rows[i][k]) * s;
		}

		return Quaternionf(temp[0], temp[1], temp[2], temp[3]);
	}
};

namespace math {

inline Vector3f rotated(const Vector3f v, const Vector3f axis, float cosine, float sine) {
	Basis3f b;
	b.set_axis_angle(axis, cosine, sine);
	return b.xform(v);
}

} // namespace math
} // namespace zylann

#endif // ZN_MATH_BASIS3F_H
