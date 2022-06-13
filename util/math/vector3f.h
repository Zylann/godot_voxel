#ifndef ZYLANN_VECTOR3F_H
#define ZYLANN_VECTOR3F_H

#include "../errors.h"
#include "funcs.h"
#include <iosfwd>

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

	// It is recommended to use `explicit` because otherwise it would open the door to plenty of implicit conversions
	// which would make many cases ambiguous.
	explicit Vector3f(float p_v) : x(p_v), y(p_v), z(p_v) {}

	Vector3f(float p_x, float p_y, float p_z) : x(p_x), y(p_y), z(p_z) {}

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

inline Vector3f min(const Vector3f a, const Vector3f b) {
	return Vector3f(min(a.x, b.x), min(a.y, b.y), min(a.z, b.z));
}

inline Vector3f max(const Vector3f a, const Vector3f b) {
	return Vector3f(max(a.x, b.x), max(a.y, b.y), max(a.z, b.z));
}

inline Vector3f floor(const Vector3f a) {
	return Vector3f(Math::floor(a.x), Math::floor(a.y), Math::floor(a.z));
}

inline Vector3f ceil(const Vector3f a) {
	return Vector3f(Math::ceil(a.x), Math::ceil(a.y), Math::ceil(a.z));
}

inline Vector3f lerp(const Vector3f a, const Vector3f b, const float t) {
	return Vector3f(Math::lerp(a.x, b.x, t), Math::lerp(a.y, b.y, t), Math::lerp(a.z, b.z, t));
}

inline bool has_nan(const Vector3f &v) {
	return Math::is_nan(v.x) || Math::is_nan(v.y) || Math::is_nan(v.z);
}

inline float length_squared(const Vector3f v) {
	return v.x * v.x + v.y * v.y + v.z * v.z;
}

inline float length(const Vector3f &v) {
	return Math::sqrt(length_squared(v));
}

inline float distance_squared(const Vector3f &a, const Vector3f &b) {
	return length_squared(b - a);
}

inline Vector3f cross(const Vector3f &a, const Vector3f &b) {
	const Vector3f ret( //
			(a.y * b.z) - (a.z * b.y), //
			(a.z * b.x) - (a.x * b.z), //
			(a.x * b.y) - (a.y * b.x));
	return ret;
}

inline float dot(const Vector3f &a, const Vector3f &b) {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vector3f normalized(const Vector3f &v) {
	const float lengthsq = length_squared(v);
	if (lengthsq == 0) {
		return Vector3f();
	} else {
		const float length = Math::sqrt(lengthsq);
		return v / length;
	}
}

inline bool is_normalized(const Vector3f &v) {
	// use length_squared() instead of length() to avoid sqrt(), makes it more stringent.
	return Math::is_equal_approx(length_squared(v), 1, float(UNIT_EPSILON));
}

inline float distance(const Vector3f &a, const Vector3f &b) {
	return Math::sqrt(distance_squared(a, b));
}

} // namespace math

std::stringstream &operator<<(std::stringstream &ss, const Vector3f &v);

} // namespace zylann

#endif // ZYLANN_VECTOR3F_H
