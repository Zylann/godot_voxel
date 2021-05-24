#ifndef VOXEL_MATH_FUNCS_H
#define VOXEL_MATH_FUNCS_H

#include <core/math/vector3.h>

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
		Vector3 position) {
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

template <typename T>
inline T min(const T a, const T b) {
	return a < b ? a : b;
}

template <typename T>
inline T max(const T a, const T b) {
	return a > b ? a : b;
}

template <typename T>
inline T min(const T a, const T b, const T c, const T d) {
	return min(min(a, b), min(c, d));
}

template <typename T>
inline T max(const T a, const T b, const T c, const T d) {
	return max(max(a, b), max(c, d));
}

template <typename T>
inline T min(const T a, const T b, const T c, const T d, const T e, const T f) {
	return min(min(min(a, b), min(c, d)), min(e, f));
}

template <typename T>
inline T max(const T a, const T b, const T c, const T d, const T e, const T f) {
	return max(max(max(a, b), max(c, d)), max(e, f));
}

template <typename T>
inline T min(const T a, const T b, const T c, const T d, const T e, const T f, const T g, const T h) {
	return min(min(a, b, c, d), min(e, f, g, h));
}

template <typename T>
inline T max(const T a, const T b, const T c, const T d, const T e, const T f, const T g, const T h) {
	return max(max(a, b, c, d), max(e, f, g, h));
}

template <typename T>
inline T clamp(const T x, const T min_value, const T max_value) {
	// TODO Optimization: clang can optimize a min/max implementation. Worth changing to that?
	if (x < min_value) {
		return min_value;
	}
	if (x > max_value) {
		return max_value;
	}
	return x;
}

template <typename T>
inline T squared(const T x) {
	return x * x;
}

// TODO Rename udiv => floordiv

// Performs euclidean division, aka floored division.
// This implementation expects a strictly positive divisor.
//
//    x   | `/` | udiv
// ----------------------
//    -6  | -2  | -2
//    -5  | -1  | -2
//    -4  | -1  | -2
//    -3  | -1  | -1
//    -2  | 0   | -1
//    -1  | 0   | -1
//    0   | 0   | 0
//    1   | 0   | 0
//    2   | 0   | 0
//    3   | 1   | 1
//    4   | 1   | 1
//    5   | 1   | 1
//    6   | 2   | 2
inline int udiv(int x, int d) {
#ifdef DEBUG_ENABLED
	CRASH_COND(d < 0);
#endif
	if (x < 0) {
		return (x - d + 1) / d;
	} else {
		return x / d;
	}
}

inline int ceildiv(int x, int d) {
	return -udiv(-x, d);
}

// TODO Rename `wrapi`
// `Math::wrapi` with zero min
inline int wrap(int x, int d) {
	return ((unsigned int)x - (x < 0)) % (unsigned int)d;
	//return ((x % d) + d) % d;
}

// Math::wrapf with zero min
inline float wrapf(float x, float d) {
	return Math::is_zero_approx(d) ? 0.f : x - (d * Math::floor(x / d));
}

// Similar to Math::smoothstep but doesn't use macro to clamp
inline float smoothstep(float p_from, float p_to, float p_weight) {
	if (Math::is_equal_approx(p_from, p_to)) {
		return p_from;
	}
	float x = clamp((p_weight - p_from) / (p_to - p_from), 0.0f, 1.0f);
	return x * x * (3.0f - 2.0f * x);
}

inline float fract(float x) {
	return x - Math::floor(x);
}

inline Vector3 fract(const Vector3 &p) {
	return Vector3(fract(p.x), fract(p.y), fract(p.z));
}

// inline bool is_power_of_two(int i) {
// 	return i & (i - 1);
// }

#endif // VOXEL_MATH_FUNCS_H
