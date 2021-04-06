#ifndef VOXEL_MATH_SDF_H
#define VOXEL_MATH_SDF_H

#include "interval.h"
#include <core/math/vector2.h>

// Signed-distance-field functions.
// For more, see https://www.iquilezles.org/www/articles/distfunctions/distfunctions.htm

inline float sdf_box(const Vector3 pos, const Vector3 extents) {
	Vector3 d = pos.abs() - extents;
	return min(max(d.x, max(d.y, d.z)), 0.f) +
		   Vector3(max(d.x, 0.f), max(d.y, 0.f), max(d.z, 0.f)).length();
}

inline Interval sdf_box(
		const Interval &x, const Interval &y, const Interval &z,
		const Interval &sx, const Interval &sy, const Interval &sz) {
	Interval dx = abs(x) - sx;
	Interval dy = abs(y) - sy;
	Interval dz = abs(z) - sz;
	return min_interval(max_interval(dx, max_interval(dy, dz)), 0.f) +
		   get_length(max_interval(dx, 0.f), max_interval(dy, 0.f), max_interval(dz, 0.f));
}

inline float sdf_torus(float x, float y, float z, float r0, float r1) {
	Vector2 q = Vector2(Vector2(x, z).length() - r0, y);
	return q.length() - r1;
}

inline Interval sdf_torus(
		const Interval &x, const Interval &y, const Interval &z, const Interval r0, const Interval r1) {
	Interval qx = get_length(x, z) - r0;
	return get_length(qx, y) - r1;
}

inline float sdf_union(float a, float b) {
	return min(a, b);
}

// Subtracts SDF b from SDF a
inline float sdf_subtract(float a, float b) {
	return max(a, -b);
}

inline float sdf_smooth_union(float a, float b, float s) {
	float h = clamp(0.5f + 0.5f * (b - a) / s, 0.0f, 1.0f);
	return Math::lerp(b, a, h) - s * h * (1.0f - h);
}

// Inverted a and b because it subtracts SDF a from SDF b
inline float sdf_smooth_subtract(float b, float a, float s) {
	float h = clamp(0.5f - 0.5f * (b + a) / s, 0.0f, 1.0f);
	return Math::lerp(b, -a, h) + s * h * (1.0f - h);
}

#endif // VOXEL_MATH_SDF_H
