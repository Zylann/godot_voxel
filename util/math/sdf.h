#ifndef ZN_MATH_SDF_H
#define ZN_MATH_SDF_H

#include "interval.h"
#include "vector2.h"
#include "vector3.h"

namespace zylann::math {

// Signed-distance-field functions.
// For more, see https://www.iquilezles.org/www/articles/distfunctions/distfunctions.htm

// TODO Use `float`, or templatize SDF values. Doubles may prevent some SIMD optimizations.

inline real_t sdf_box(const Vector3 pos, const Vector3 extents) {
	const Vector3 d = pos.abs() - extents;
	return minf(maxf(d.x, maxf(d.y, d.z)), 0) + Vector3(maxf(d.x, 0), maxf(d.y, 0), maxf(d.z, 0)).length();
}

inline Interval sdf_box(const Interval &x, const Interval &y, const Interval &z, const Interval &sx, const Interval &sy,
		const Interval &sz) {
	const Interval dx = abs(x) - sx;
	const Interval dy = abs(y) - sy;
	const Interval dz = abs(z) - sz;
	return min_interval(max_interval(dx, max_interval(dy, dz)), 0) +
			get_length(max_interval(dx, 0), max_interval(dy, 0), max_interval(dz, 0));
}

inline real_t sdf_sphere(Vector3 pos, Vector3 center, real_t radius) {
	return pos.distance_to(center) - radius;
}

inline real_t sdf_torus(real_t x, real_t y, real_t z, real_t r0, real_t r1) {
	const Vector2 q = Vector2(Vector2(x, z).length() - r0, y);
	return q.length() - r1;
}

inline Interval sdf_torus(
		const Interval &x, const Interval &y, const Interval &z, const Interval r0, const Interval r1) {
	const Interval qx = get_length(x, z) - r0;
	return get_length(qx, y) - r1;
}

// Note: calculate `plane_d` as `dot(plane_normal, point_in_plane)`
inline real_t sdf_plane(Vector3 pos, Vector3 plane_normal, real_t plane_d) {
	// On Inigo's website it's a `+h`, but it seems to be backward because then if a plane has normal (0,1,0) and height
	// 1, a point at (0,1,0) will give a dot of 1, + height will be 2, which is wrong because the expected SDF here
	// would be 0.
	return pos.dot(plane_normal) - plane_d;
}

inline real_t sdf_union(real_t a, real_t b) {
	return min(a, b);
}

// Subtracts SDF b from SDF a
inline real_t sdf_subtract(real_t a, real_t b) {
	return max(a, -b);
}

inline real_t sdf_smooth_union(real_t a, real_t b, real_t s) {
	const real_t h = clampf(0.5 + 0.5 * (b - a) / s, 0.0, 1.0);
	return Math::lerp(b, a, h) - s * h * (1.0 - h);
}

// Inverted a and b because it subtracts SDF a from SDF b
inline real_t sdf_smooth_subtract(real_t b, real_t a, real_t s) {
	const real_t h = clampf(0.5 - 0.5 * (b + a) / s, 0.0, 1.0);
	return Math::lerp(b, -a, h) + s * h * (1.0 - h);
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

inline real_t sdf_round_cone(Vector3 p, Vector3 a, Vector3 b, real_t r1, real_t r2) {
	// sampling independent computations (only depend on shape)
	const Vector3 ba = b - a;
	const real_t l2 = dot(ba, ba);
	const real_t rr = r1 - r2;
	const real_t a2 = l2 - rr * rr;
	const real_t il2 = 1.0 / l2;

	// sampling dependant computations
	const Vector3 pa = p - a;
	const real_t y = dot(pa, ba);
	const real_t z = y - l2;
	const real_t x2 = length_squared(pa * l2 - ba * y);
	const real_t y2 = y * y * l2;
	const real_t z2 = z * z * l2;

	// single square root!
	const real_t k = sign(rr) * rr * rr * x2;
	if (sign(z) * a2 * z2 > k) {
		return Math::sqrt(x2 + z2) * il2 - r2;
	}
	if (sign(y) * a2 * y2 < k) {
		return Math::sqrt(x2 + y2) * il2 - r1;
	}
	return (Math::sqrt(x2 * a2 * il2) + y * rr) * il2 - r1;
}

struct SdfRoundConePrecalc {
	Vector3 a;
	Vector3 b;
	real_t r1;
	real_t r2;

	Vector3 ba;
	real_t l2;
	real_t rr;
	real_t a2;
	real_t il2;

	void update() {
		ba = b - a;
		l2 = dot(ba, ba);
		rr = r1 - r2;
		a2 = l2 - rr * rr;
		il2 = 1.0 / l2;
	}

	real_t operator()(Vector3 p) const {
		const Vector3 pa = p - a;
		const real_t y = dot(pa, ba);
		const real_t z = y - l2;
		const real_t x2 = length_squared(pa * l2 - ba * y);
		const real_t y2 = y * y * l2;
		const real_t z2 = z * z * l2;

		const real_t k = sign(rr) * rr * rr * x2;
		if (sign(z) * a2 * z2 > k) {
			return Math::sqrt(x2 + z2) * il2 - r2;
		}
		if (sign(y) * a2 * y2 < k) {
			return Math::sqrt(x2 + y2) * il2 - r1;
		}
		return (Math::sqrt(x2 * a2 * il2) + y * rr) * il2 - r1;
	}
};

} // namespace zylann::math

#endif // ZN_MATH_SDF_H
