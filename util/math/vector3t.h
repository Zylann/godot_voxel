#ifndef ZYLANN_VECTOR3T_H
#define ZYLANN_VECTOR3T_H

#include "../errors.h"
#include "funcs.h"

namespace zylann {

// Template 3-dimensional vector. Only fields and standard operators.
// Math functions are separate to allow more unified overloading, and similarity with other math libraries such as
// shaders.
template <typename T>
struct Vector3T {
	enum Axis { //
		AXIS_X = 0,
		AXIS_Y = 1,
		AXIS_Z = 2,
		AXIS_COUNT = 3
	};

	union {
		struct {
			T x;
			T y;
			T z;
		};
		T coords[3];
	};

	Vector3T() : x(0), y(0), z(0) {}

	// It is recommended to use `explicit` because otherwise it would open the door to plenty of implicit conversions
	// which would make many cases ambiguous.
	explicit Vector3T(T p_v) : x(p_v), y(p_v), z(p_v) {}

	Vector3T(T p_x, T p_y, T p_z) : x(p_x), y(p_y), z(p_z) {}

	inline const T &operator[](const unsigned int p_axis) const {
#ifdef DEBUG_ENABLED
		ZN_ASSERT(p_axis < AXIS_COUNT);
#endif
		return coords[p_axis];
	}

	inline T &operator[](const unsigned int p_axis) {
#ifdef DEBUG_ENABLED
		ZN_ASSERT(p_axis < AXIS_COUNT);
#endif
		return coords[p_axis];
	}

	inline Vector3T &operator+=(const Vector3T &p_v) {
		x += p_v.x;
		y += p_v.y;
		z += p_v.z;
		return *this;
	}

	inline Vector3T operator+(const Vector3T &p_v) const {
		return Vector3T(x + p_v.x, y + p_v.y, z + p_v.z);
	}

	inline Vector3T &operator-=(const Vector3T &p_v) {
		x -= p_v.x;
		y -= p_v.y;
		z -= p_v.z;
		return *this;
	}

	inline Vector3T operator-(const Vector3T &p_v) const {
		return Vector3T(x - p_v.x, y - p_v.y, z - p_v.z);
	}

	inline Vector3T &operator*=(const Vector3T &p_v) {
		x *= p_v.x;
		y *= p_v.y;
		z *= p_v.z;
		return *this;
	}

	inline Vector3T operator*(const Vector3T &p_v) const {
		return Vector3T(x * p_v.x, y * p_v.y, z * p_v.z);
	}

	inline Vector3T &operator/=(const Vector3T &p_v) {
		x /= p_v.x;
		y /= p_v.y;
		z /= p_v.z;
		return *this;
	}

	inline Vector3T operator/(const Vector3T &p_v) const {
		return Vector3T(x / p_v.x, y / p_v.y, z / p_v.z);
	}

	inline Vector3T &operator*=(const T p_scalar) {
		x *= p_scalar;
		y *= p_scalar;
		z *= p_scalar;
		return *this;
	}

	inline Vector3T operator*(const T p_scalar) const {
		return Vector3T(x * p_scalar, y * p_scalar, z * p_scalar);
	}

	inline Vector3T &operator/=(const T p_scalar) {
		x /= p_scalar;
		y /= p_scalar;
		z /= p_scalar;
		return *this;
	}

	inline Vector3T operator/(const T p_scalar) const {
		return Vector3T(x / p_scalar, y / p_scalar, z / p_scalar);
	}

	inline Vector3T operator-() const {
		return Vector3T(-x, -y, -z);
	}

	inline bool operator==(const Vector3T &p_v) const {
		return x == p_v.x && y == p_v.y && z == p_v.z;
	}

	inline bool operator!=(const Vector3T &p_v) const {
		return x != p_v.x || y != p_v.y || z != p_v.z;
	}

	inline bool operator<(const Vector3T &p_v) const {
		if (x == p_v.x) {
			if (y == p_v.y) {
				return z < p_v.z;
			}
			return y < p_v.y;
		}
		return x < p_v.x;
	}
};

template <typename T>
inline Vector3T<T> operator*(T p_scalar, const Vector3T<T> &v) {
	return v * p_scalar;
}

namespace math {

template <typename T>
inline Vector3T<T> min(const Vector3T<T> a, const Vector3T<T> b) {
	return Vector3T<T>(min(a.x, b.x), min(a.y, b.y), min(a.z, b.z));
}

template <typename T>
inline Vector3T<T> max(const Vector3T<T> a, const Vector3T<T> b) {
	return Vector3T<T>(max(a.x, b.x), max(a.y, b.y), max(a.z, b.z));
}

template <typename T>
inline Vector3T<T> clamp(const Vector3T<T> v, const Vector3T<T> minv, const Vector3T<T> maxv) {
	return Vector3T<T>(clamp(v.x, minv.x, maxv.x), clamp(v.y, minv.y, maxv.y), clamp(v.z, minv.z, maxv.z));
}

template <typename T>
inline T length_squared(const Vector3T<T> v) {
	return v.x * v.x + v.y * v.y + v.z * v.z;
}

template <typename T>
inline T length(const Vector3T<T> &v) {
	return Math::sqrt(length_squared(v));
}

template <typename T>
inline T distance_squared(const Vector3T<T> &a, const Vector3T<T> &b) {
	return length_squared(b - a);
}

template <typename T>
inline T distance(const Vector3T<T> &a, const Vector3T<T> &b) {
	return Math::sqrt(distance_squared(a, b));
}

template <typename T>
inline Vector3T<T> cross(const Vector3T<T> &a, const Vector3T<T> &b) {
	const Vector3T<T> ret( //
			(a.y * b.z) - (a.z * b.y), //
			(a.z * b.x) - (a.x * b.z), //
			(a.x * b.y) - (a.y * b.x));
	return ret;
}

template <typename T>
inline T dot(const Vector3T<T> &a, const Vector3T<T> &b) {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

template <typename T>
inline Vector3T<T> abs(const Vector3T<T> v) {
	return Vector3T<T>(Math::abs(v.x), Math::abs(v.y), Math::abs(v.z));
}

template <typename T>
inline typename Vector3T<T>::Axis get_longest_axis(Vector3T<T> v) {
	v = abs(v);
	if (v.x > v.y) {
		if (v.x > v.z) {
			return Vector3T<T>::AXIS_X;
		} else {
			return Vector3T<T>::AXIS_Z;
		}
	} else if (v.y > v.z) {
		return Vector3T<T>::AXIS_Y;
	}
	return Vector3T<T>::AXIS_Z;
}

} // namespace math
} // namespace zylann

#endif // ZYLANN_VECTOR3F_H
