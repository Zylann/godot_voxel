#ifndef ZN_VECTOR2T_H
#define ZN_VECTOR2T_H

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

} // namespace math
} // namespace zylann

#endif // ZN_VECTOR2T_H
