#ifndef ZYLANN_VECTOR2F_H
#define ZYLANN_VECTOR2F_H

#include <core/error/error_macros.h>

namespace zylann {

// 32-bit float precision 2D vector.
// Because Godot's `Vector2` uses `real_t`, so when `real_t` is `double` it forces some things to use double-precision
// vectors while they dont need that amount of precision.
struct Vector2f {
	static const unsigned int AXIS_X = 0;
	static const unsigned int AXIS_Y = 1;
	static const unsigned int AXIS_COUNT = 2;

	union {
		struct {
			float x;
			float y;
		};
		float coords[2];
	};

	Vector2f() : x(0), y(0) {}
	Vector2f(float p_x, float p_y) : x(p_x), y(p_y) {}

	float cross(const Vector2f &p_other) const {
		return x * p_other.y - y * p_other.x;
	}

	inline const float &operator[](const unsigned int p_axis) const {
#ifdef DEBUG_ENABLED
		CRASH_COND(p_axis >= AXIS_COUNT);
#endif
		return coords[p_axis];
	}

	inline float &operator[](const unsigned int p_axis) {
#ifdef DEBUG_ENABLED
		CRASH_COND(p_axis >= AXIS_COUNT);
#endif
		return coords[p_axis];
	}

	inline Vector2f &operator+=(const Vector2f &p_v) {
		x += p_v.x;
		y += p_v.y;
		return *this;
	}

	inline Vector2f operator+(const Vector2f &p_v) const {
		return Vector2f(x + p_v.x, y + p_v.y);
	}

	inline Vector2f operator-(const Vector2f &p_v) const {
		return Vector2f(x - p_v.x, y - p_v.y);
	}

	inline Vector2f operator*(const Vector2f &p_v) const {
		return Vector2f(x * p_v.x, y * p_v.y);
	}

	inline Vector2f &operator*=(const float p_scalar) {
		x *= p_scalar;
		y *= p_scalar;
		return *this;
	}

	inline Vector2f operator*(const float p_scalar) const {
		return Vector2f(x * p_scalar, y * p_scalar);
	}
};

inline Vector2f operator*(float p_scalar, const Vector2f &v) {
	return v * p_scalar;
}

} // namespace zylann

#endif // ZYLANN_VECTOR2F_H
