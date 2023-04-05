#ifndef ZN_VECTOR2T_H
#define ZN_VECTOR2T_H

#include "funcs.h"

namespace zylann {

template <typename T>
struct Vector2T {
	static const unsigned int AXIS_X = 0;
	static const unsigned int AXIS_Y = 1;
	static const unsigned int AXIS_COUNT = 2;

	union {
		struct {
			T x;
			T y;
		};
		T coords[2];
	};

	Vector2T() : x(0), y(0) {}

	// It is recommended to use `explicit` because otherwise it would open the door to plenty of implicit conversions
	// which would make many cases ambiguous.
	explicit Vector2T(T p_v) : x(p_v), y(p_v) {}

	Vector2T(T p_x, T p_y) : x(p_x), y(p_y) {}

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

	inline Vector2T &operator+=(const Vector2T &p_v) {
		x += p_v.x;
		y += p_v.y;
		return *this;
	}

	inline Vector2T operator+(const Vector2T &p_v) const {
		return Vector2T(x + p_v.x, y + p_v.y);
	}

	inline Vector2T operator-(const Vector2T &p_v) const {
		return Vector2T(x - p_v.x, y - p_v.y);
	}

	inline Vector2T operator*(const Vector2T &p_v) const {
		return Vector2T(x * p_v.x, y * p_v.y);
	}

	inline Vector2T &operator*=(const T p_scalar) {
		x *= p_scalar;
		y *= p_scalar;
		return *this;
	}

	inline Vector2T operator*(const T p_scalar) const {
		return Vector2T(x * p_scalar, y * p_scalar);
	}

	inline Vector2T operator/(const Vector2T &p_v) const {
		return Vector2T(x / p_v.x, y / p_v.y);
	}

	inline Vector2T operator/(const T p_scalar) const {
		return Vector2T(x / p_scalar, y / p_scalar);
	}

	inline bool operator==(const Vector2T &p_v) const {
		return x == p_v.x && y == p_v.y;
	}

	inline bool operator!=(const Vector2T &p_v) const {
		return x != p_v.x || y != p_v.y;
	}
};

template <typename T>
inline Vector2T<T> operator*(T p_scalar, const Vector2T<T> &v) {
	return v * p_scalar;
}

namespace math {

template <typename T>
T cross(const Vector2T<T> &a, const Vector2T<T> &b) {
	return a.x * b.y - a.y * b.x;
}

template <typename T>
inline Vector2T<T> abs(const Vector2T<T> v) {
	return Vector2T<T>(Math::abs(v.x), Math::abs(v.y));
}

template <typename T>
inline Vector2T<T> sign_nonzero(Vector2T<T> v) {
	return Vector2T<T>(sign_nonzero(v.x), sign_nonzero(v.y));
}

template <typename T>
inline T dot(const Vector2T<T> &a, const Vector2T<T> &b) {
	return a.x * b.x + a.y * b.y;
}

template <typename T>
inline T length_squared(const Vector2T<T> v) {
	return v.x * v.x + v.y * v.y;
}

template <typename T>
inline T distance_squared(const Vector2T<T> &a, const Vector2T<T> &b) {
	return length_squared(b - a);
}

} // namespace math
} // namespace zylann

#endif // ZN_VECTOR2T_H
