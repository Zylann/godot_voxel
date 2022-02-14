#ifndef VOXEL_MATH_SDF_H
#define VOXEL_MATH_SDF_H

#include "interval.h"
#include <core/math/vector2.h>

namespace zylann::math {

// Signed-distance-field functions.
// For more, see https://www.iquilezles.org/www/articles/distfunctions/distfunctions.htm

inline real_t sdf_box(const Vector3 pos, const Vector3 extents) {
	Vector3 d = pos.abs() - extents;
	return minf(maxf(d.x, maxf(d.y, d.z)), 0.f) + Vector3(maxf(d.x, 0.f), maxf(d.y, 0.f), maxf(d.z, 0.f)).length();
}

inline Interval sdf_box(const Interval &x, const Interval &y, const Interval &z, const Interval &sx, const Interval &sy,
		const Interval &sz) {
	Interval dx = abs(x) - sx;
	Interval dy = abs(y) - sy;
	Interval dz = abs(z) - sz;
	return min_interval(max_interval(dx, max_interval(dy, dz)), 0.f) +
			get_length(max_interval(dx, 0.f), max_interval(dy, 0.f), max_interval(dz, 0.f));
}

inline real_t sdf_sphere(Vector3 pos, Vector3 center, real_t radius) {
	return pos.distance_to(center) - radius;
}

inline real_t sdf_torus(real_t x, real_t y, real_t z, real_t r0, real_t r1) {
	Vector2 q = Vector2(Vector2(x, z).length() - r0, y);
	return q.length() - r1;
}

inline Interval sdf_torus(
		const Interval &x, const Interval &y, const Interval &z, const Interval r0, const Interval r1) {
	Interval qx = get_length(x, z) - r0;
	return get_length(qx, y) - r1;
}

inline real_t sdf_union(real_t a, real_t b) {
	return min(a, b);
}

// Subtracts SDF b from SDF a
inline real_t sdf_subtract(real_t a, real_t b) {
	return max(a, -b);
}

inline real_t sdf_smooth_union(real_t a, real_t b, real_t s) {
	const real_t h = clampf(0.5f + 0.5f * (b - a) / s, 0.0f, 1.0f);
	return Math::lerp(b, a, h) - s * h * (1.0f - h);
}

// Inverted a and b because it subtracts SDF a from SDF b
inline real_t sdf_smooth_subtract(real_t b, real_t a, real_t s) {
	const real_t h = clampf(0.5f - 0.5f * (b + a) / s, 0.0f, 1.0f);
	return Math::lerp(b, -a, h) + s * h * (1.0f - h);
}

inline Interval sdf_union(Interval a, Interval b) {
	return min_interval(a, b);
}

// Does a - b
inline Interval sdf_subtract(Interval a, Interval b) {
	return max_interval(a, -b);
}

Interval sdf_smooth_union(Interval p_b, Interval p_a, real_t p_s);

// Does b - a
Interval sdf_smooth_subtract(Interval p_b, Interval p_a, real_t p_s);

enum SdfAffectingArguments { //
	SDF_ONLY_A,
	SDF_ONLY_B,
	SDF_BOTH
};

// Tests which argument can affect the result.
// for a - b
SdfAffectingArguments sdf_subtract_side(Interval a, Interval b);
// for a - b
SdfAffectingArguments sdf_polynomial_smooth_subtract_side(Interval a, Interval b, real_t s);

SdfAffectingArguments sdf_union_side(Interval a, Interval b);
SdfAffectingArguments sdf_polynomial_smooth_union_side(Interval a, Interval b, real_t s);

} // namespace zylann::math

#endif // VOXEL_MATH_SDF_H
