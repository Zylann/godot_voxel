#ifndef ZYLANN_VECTOR2F_H
#define ZYLANN_VECTOR2F_H

#include "../errors.h"
#include "vector2t.h"

namespace zylann {

// 32-bit float precision 2D vector.
// Because Godot's `Vector2` uses `real_t`, so when `real_t` is `double` it forces some things to use double-precision
// vectors while they dont need that amount of precision.
typedef Vector2T<float> Vector2f;

namespace math {

// Float version of Geometry::is_point_in_triangle()
inline bool is_point_in_triangle(const Vector2f &s, const Vector2f &a, const Vector2f &b, const Vector2f &c) {
	const Vector2f an = a - s;
	const Vector2f bn = b - s;
	const Vector2f cn = c - s;

	const bool orientation = cross(an, bn) > 0;

	if ((cross(bn, cn) > 0) != orientation) {
		return false;
	}

	return (cross(cn, an) > 0) == orientation;
}

} // namespace math
} // namespace zylann

#endif // ZYLANN_VECTOR2F_H
