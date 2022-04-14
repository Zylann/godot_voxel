#ifndef ZYLANN_VECTOR3F_H
#define ZYLANN_VECTOR3F_H

#include "../errors.h"
#include <core/math/math_funcs.h>

namespace zylann {

// 32-bit float precision 3D vector.
// Because Godot's `Vector3` uses `real_t`, so when `real_t` is `double` it forces some things to use double-precision
// vectors while they dont need that amount of precision. This is also a problem for some third-party libraries
// that do not support `double` as a result.
struct Vector3f {
	static const unsigned int AXIS_X = 0;
	static const unsigned int AXIS_Y = 1;
	static const unsigned int AXIS_Z = 2;
	static const unsigned int AXIS_COUNT = 3;

	union {
		struct {
			float x;
			float y;
			float z;
		};
		float coords[3];
	};

	Vector3f() : x(0), y(0), z(0) {}
	Vector3f(float p_x, float p_y, float p_z) : x(p_x), y(p_y), z(p_z) {}

	inline float length_squared() const {
		return x * x + y * y + z * z;
	}

	inline float length() const {
		return Math::sqrt(length_squared());
	}

	inline float distance_squared_to(const Vector3f &p_to) const {
		return (p_to - *this).length_squared();
	}

	inline Vector3f cross(const Vector3f &p_with) const {
		const Vector3f ret( //
				(y * p_with.z) - (z * p_with.y), //
				(z * p_with.x) - (x * p_with.z), //
				(x * p_with.y) - (y * p_with.x));
		return ret;
	}

	inline float dot(const Vector3f &p_with) const {
		return x * p_with.x + y * p_with.y + z * p_with.z;
	}

	inline void normalize() {
		const float lengthsq = length_squared();
		if (lengthsq == 0) {
			x = y = z = 0;
		} else {
			const float length = Math::sqrt(lengthsq);
			x /= length;
			y /= length;
			z /= length;
		}
	}

	inline Vector3f normalized() const {
		Vector3f v = *this;
		v.normalize();
		return v;
	}

	inline const float &operator[](const unsigned int p_axis) const {
#ifdef DEBUG_ENABLED
		ZN_ASSERT(p_axis < AXIS_COUNT);
#endif
		return coords[p_axis];
	}

	inline float &operator[](const unsigned int p_axis) {
#ifdef DEBUG_ENABLED
		ZN_ASSERT(p_axis < AXIS_COUNT);
#endif
		return coords[p_axis];
	}

	inline Vector3f &operator+=(const Vector3f &p_v) {
		x += p_v.x;
		y += p_v.y;
		z += p_v.z;
		return *this;
	}

	inline Vector3f operator+(const Vector3f &p_v) const {
		return Vector3f(x + p_v.x, y + p_v.y, z + p_v.z);
	}

	inline Vector3f &operator-=(const Vector3f &p_v) {
		x -= p_v.x;
		y -= p_v.y;
		z -= p_v.z;
		return *this;
	}

	inline Vector3f operator-(const Vector3f &p_v) const {
		return Vector3f(x - p_v.x, y - p_v.y, z - p_v.z);
	}

	inline Vector3f &operator*=(const Vector3f &p_v) {
		x *= p_v.x;
		y *= p_v.y;
		z *= p_v.z;
		return *this;
	}

	inline Vector3f operator*(const Vector3f &p_v) const {
		return Vector3f(x * p_v.x, y * p_v.y, z * p_v.z);
	}

	inline Vector3f &operator/=(const Vector3f &p_v) {
		x /= p_v.x;
		y /= p_v.y;
		z /= p_v.z;
		return *this;
	}

	inline Vector3f operator/(const Vector3f &p_v) const {
		return Vector3f(x / p_v.x, y / p_v.y, z / p_v.z);
	}

	inline Vector3f &operator*=(const float p_scalar) {
		x *= p_scalar;
		y *= p_scalar;
		z *= p_scalar;
		return *this;
	}

	inline Vector3f operator*(const float p_scalar) const {
		return Vector3f(x * p_scalar, y * p_scalar, z * p_scalar);
	}

	inline Vector3f &operator/=(const float p_scalar) {
		x /= p_scalar;
		y /= p_scalar;
		z /= p_scalar;
		return *this;
	}

	inline Vector3f operator/(const float p_scalar) const {
		return Vector3f(x / p_scalar, y / p_scalar, z / p_scalar);
	}

	inline Vector3f operator-() const {
		return Vector3f(-x, -y, -z);
	}

	inline bool operator==(const Vector3f &p_v) const {
		return x == p_v.x && y == p_v.y && z == p_v.z;
	}

	inline bool operator!=(const Vector3f &p_v) const {
		return x != p_v.x || y != p_v.y || z != p_v.z;
	}

	inline bool operator<(const Vector3f &p_v) const {
		if (x == p_v.x) {
			if (y == p_v.y) {
				return z < p_v.z;
			}
			return y < p_v.y;
		}
		return x < p_v.x;
	}
};

inline Vector3f operator*(float p_scalar, const Vector3f &v) {
	return v * p_scalar;
}

namespace math {

// Trilinear interpolation between corner values of a cube.
//
//      6---------------7
//     /|              /|
//    / |             / |
//   5---------------4  |
//   |  |            |  |
//   |  |            |  |
//   |  |            |  |
//   |  2------------|--3        Y
//   | /             | /         | Z
//   |/              |/          |/
//   1---------------0      X----o
//
template <typename T>
inline T interpolate(const T v0, const T v1, const T v2, const T v3, const T v4, const T v5, const T v6, const T v7,
		Vector3f position) {
	const float one_min_x = 1.f - position.x;
	const float one_min_y = 1.f - position.y;
	const float one_min_z = 1.f - position.z;
	const float one_min_x_one_min_y = one_min_x * one_min_y;
	const float x_one_min_y = position.x * one_min_y;

	T res = one_min_z * (v0 * one_min_x_one_min_y + v1 * x_one_min_y + v4 * one_min_x * position.y);
	res += position.z * (v3 * one_min_x_one_min_y + v2 * x_one_min_y + v7 * one_min_x * position.y);
	res += position.x * position.y * (v5 * one_min_z + v6 * position.z);

	return res;
}

} // namespace math
} // namespace zylann

#endif // ZYLANN_VECTOR3F_H
