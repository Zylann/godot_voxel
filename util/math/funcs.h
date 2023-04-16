#ifndef VOXEL_MATH_FUNCS_H
#define VOXEL_MATH_FUNCS_H

#include "../errors.h"

#if defined(ZN_GODOT)
#include <core/math/math_funcs.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/core/math.hpp>
using namespace godot;
#endif

#include "constants.h"
#include <float.h> // for `_isnan`

namespace zylann::math {

// Generic math functions, only using scalar types.

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

// template versions require explicit types.
// float versions do not require casting all the time, so optional double-precision support with `real_t` is easier when
// using arguments of different precision.

inline float minf(float a, float b) {
	return a < b ? a : b;
}

inline double minf(double a, double b) {
	return a < b ? a : b;
}

inline float maxf(float a, float b) {
	return a > b ? a : b;
}

inline double maxf(double a, double b) {
	return a > b ? a : b;
}

template <typename T>
inline T clamp(const T x, const T min_value, const T max_value) {
	// TODO Optimization: clang can optimize a min/max implementation. Worth changing to that?
	// TODO Enforce T as being numeric
	if (x < min_value) {
		return min_value;
	}
	if (x > max_value) {
		return max_value;
	}
	return x;
}

inline float clampf(float x, float min_value, float max_value) {
	return min(max(x, min_value), max_value);
}

inline double clampf(double x, double min_value, double max_value) {
	return min(max(x, min_value), max_value);
}

template <typename T>
inline T squared(const T x) {
	return x * x;
}

template <typename T>
inline T cubed(const T x) {
	return x * x * x;
}

// Performs euclidean division, aka floored division.
// This implementation expects a strictly positive divisor.
//
// Example with division by 3:
//
//    x   | `/` | floordiv | ceildiv
// ----------------------------------
//    -6  | -2  | -2       | -2
//    -5  | -1  | -2       | -1
//    -4  | -1  | -2       | -1
//    -3  | -1  | -1       | -1
//    -2  | 0   | -1       | 0
//    -1  | 0   | -1       | 0
//    0   | 0   | 0        | 0
//    1   | 0   | 0        | 1
//    2   | 0   | 0        | 1
//    3   | 1   | 1        | 1
//    4   | 1   | 1        | 2
//    5   | 1   | 1        | 2
//    6   | 2   | 2        | 2
inline int floordiv(int x, int d) {
#ifdef DEBUG_ENABLED
	ZN_ASSERT(d > 0);
#endif
	if (x < 0) {
		return (x - d + 1) / d;
	} else {
		return x / d;
	}
}

// ceildiv(0, 10) == 0
// ceildiv(1, 10) == 1
// ceildiv(5, 10) == 1
// ceildiv(10, 10) == 1
// ceildiv(11, 10) == 2
inline int ceildiv(int x, int d) {
#ifdef DEBUG_ENABLED
	ZN_ASSERT(d > 0);
#endif
	if (x > 0) {
		return (x + d - 1) / d;
	} else {
		return x / d;
	}
	// return -floordiv(-x, d);
}

// TODO Rename `wrapi`
// `Math::wrapi` with zero min
inline int wrap(int x, int d) {
	return ((unsigned int)x - (x < 0)) % (unsigned int)d;
	// return ((x % d) + d) % d;
}

// Math::wrapf with zero min
inline float wrapf(float x, float d) {
	return Math::is_zero_approx(d) ? 0.f : x - (d * Math::floor(x / d));
}

inline double wrapf(double x, double d) {
	return Math::is_zero_approx(d) ? 0.0 : x - (d * Math::floor(x / d));
}

// Similar to Math::smoothstep but doesn't use macro to clamp
inline float smoothstep(float p_from, float p_to, float p_weight) {
	if (Math::is_equal_approx(p_from, p_to)) {
		return p_from;
	}
	float x = clamp((p_weight - p_from) / (p_to - p_from), 0.0f, 1.0f);
	return x * x * (3.0f - 2.0f * x);
}

inline double smoothstep(double p_from, double p_to, double p_weight) {
	if (Math::is_equal_approx(p_from, p_to)) {
		return p_from;
	}
	double x = clamp((p_weight - p_from) / (p_to - p_from), 0.0, 1.0);
	return x * x * (3.0 - 2.0 * x);
}

inline float fract(float x) {
	return x - Math::floor(x);
}

inline double fract(double x) {
	return x - Math::floor(x);
}

inline bool is_power_of_two(size_t x) {
	return x != 0 && (x & (x - 1)) == 0;
}

// If `x` is a power of two, returns `x`.
// Otherwise, returns the closest power of two greater than `x`.
inline unsigned int get_next_power_of_two_32(unsigned int x) {
	if (x == 0) {
		return 0;
	}
	--x;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	return ++x;
}

// Function to find the previous power of 2 to an integer.
inline unsigned int get_previous_power_of_two_32(unsigned int x) {
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	return x - (x >> 1);
}

// Assuming `pot == (1 << i)`, returns `i`.
inline unsigned int get_shift_from_power_of_two_32(unsigned int pot) {
#ifdef DEBUG_ENABLED
	ZN_ASSERT(is_power_of_two(pot));
#endif
	for (unsigned int i = 0; i < 32; ++i) {
		if (pot == (1u << i)) {
			return i;
		}
	}
	ZN_CRASH_MSG("Input was not a valid power of two");
	return 0;
}

// If `num` == 2^N, returns N. Otherwise, returns the exponent of the next power of two.
// 0 => 0
// 1 => 0
// 2 => 1
// 3 => 2
// 4 => 2
// 5 => 3
inline unsigned int get_next_power_of_two_32_shift(unsigned int num) {
	for (unsigned int i = 0; i < 32; ++i) {
		if (num <= (1u << i)) {
			return i;
		}
	}
	ZN_CRASH_MSG("Number too big");
	return 0;
}

// If the provided address `a` is not aligned to the number of bytes specified in `align`,
// returns the next aligned address. `align` must be a power of two.
inline size_t alignup(size_t a, size_t align) {
#ifdef DEBUG_ENABLED
	ZN_ASSERT(is_power_of_two(align));
#endif
	return (a + align - 1) & ~(align - 1);
}

// inline bool is_power_of_two(int i) {
// 	return i & (i - 1);
// }

// Float equivalent of Math::snapped, which only comes in `double` variant in Godot.
inline float snappedf(float p_value, float p_step) {
	if (p_step != 0) {
		p_value = Math::floor(p_value / p_step + 0.5f) * p_step;
	}
	return p_value;
}

template <typename T>
inline void sort(T &a, T &b) {
	if (a > b) {
		std::swap(a, b);
	}
}

template <typename T>
inline void sort(T &a, T &b, T &c, T &d) {
	sort(a, b);
	sort(c, d);
	sort(a, c);
	sort(b, d);
	sort(b, c);
}

// Returns -1 if `x` is negative, and 1 otherwise.
// Contrary to a usual version like GLSL, this one returns 1 when `x` is 0, instead of 0.
template <typename T>
inline T sign_nonzero(T x) {
	return x < 0 ? -1 : 1;
}

// Trilinear interpolation between corner values of a unit-sized cube.
// `v***` arguments are corner values named as `vXYZ`, where a coordinate is 0 or 1 on the cube.
// Coordinates of `p` are in 0..1, but are not clamped so extrapolation is possible.
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
// p000, p100, p101, p001, p010, p110, p111, p011
template <typename T, typename Vec3_T>
inline T interpolate_trilinear(const T v000, const T v100, const T v101, const T v001, const T v010, const T v110,
		const T v111, const T v011, Vec3_T p) {
	//
	const T v00 = v000 + p.x * (v100 - v000);
	const T v10 = v010 + p.x * (v110 - v010);
	const T v01 = v001 + p.x * (v101 - v001);
	const T v11 = v011 + p.x * (v111 - v011);

	const T v0 = v00 + p.y * (v10 - v00);
	const T v1 = v01 + p.y * (v11 - v01);

	const T v = v0 + p.z * (v1 - v0);

	return v;
}

// Math::is_nan exists in Godot core, but not in GDExtension...
inline bool is_nan(double p_val) {
#ifdef _MSC_VER
	return _isnan(p_val);
#elif defined(__GNUC__) && __GNUC__ < 6
	union {
		uint64_t u;
		double f;
	} ieee754;
	ieee754.f = p_val;
	// (unsigned)(0x7ff0000000000001 >> 32) : 0x7ff00000
	return ((((unsigned)(ieee754.u >> 32) & 0x7fffffff) + ((unsigned)ieee754.u != 0)) > 0x7ff00000);
#else
	return isnan(p_val);
#endif
}

// Math::is_nan exists in Godot core, but not in GDExtension...
inline bool is_nan(float p_val) {
#ifdef _MSC_VER
	return _isnan(p_val);
#elif defined(__GNUC__) && __GNUC__ < 6
	union {
		uint32_t u;
		float f;
	} ieee754;
	ieee754.f = p_val;
	// -----------------------------------
	// (single-precision floating-point)
	// NaN : s111 1111 1xxx xxxx xxxx xxxx xxxx xxxx
	//     : (> 0x7f800000)
	// where,
	//   s : sign
	//   x : non-zero number
	// -----------------------------------
	return ((ieee754.u & 0x7fffffff) > 0x7f800000);
#else
	return isnan(p_val);
#endif
}

// Math::is_inf exists in Godot core, but not in GDExtension...
inline bool is_inf(double p_val) {
#ifdef _MSC_VER
	return !_finite(p_val);
// use an inline implementation of isinf as a workaround for problematic libstdc++ versions from gcc 5.x era
#elif defined(__GNUC__) && __GNUC__ < 6
	union {
		uint64_t u;
		double f;
	} ieee754;
	ieee754.f = p_val;
	return ((unsigned)(ieee754.u >> 32) & 0x7fffffff) == 0x7ff00000 && ((unsigned)ieee754.u == 0);
#else
	return isinf(p_val);
#endif
}

// Math::is_inf exists in Godot core, but not in GDExtension...
inline bool is_inf(float p_val) {
#ifdef _MSC_VER
	return !_finite(p_val);
// use an inline implementation of isinf as a workaround for problematic libstdc++ versions from gcc 5.x era
#elif defined(__GNUC__) && __GNUC__ < 6
	union {
		uint32_t u;
		float f;
	} ieee754;
	ieee754.f = p_val;
	return (ieee754.u & 0x7fffffff) == 0x7f800000;
#else
	return isinf(p_val);
#endif
}

inline double deg_to_rad(double p_y) {
	return p_y * PI_64 / 180.0;
}

inline float deg_to_rad(float p_y) {
	return p_y * PI_32 / 180.f;
}

// Given source and destination intervals, returns parameters to use in an `a*x+b` formula to apply such remap.
// If the source interval is approximatively empty, returns zero values.
inline void remap_intervals_to_linear_params(
		float min0, float max0, float min1, float max1, float &out_a, float &out_b) {
	// min1 + (max1 - min1) * (x - min0) / (max0 - min0)
	// min1 + (max1 - min1) * (x - min0) * (1/(max0 - min0))
	// min1 +       A       * (x - min0) *        B
	// min1 + A * B * (x - min0)
	// min1 + A * B * x - A * B * min0
	// min1 +   C   * x -   C   * min0
	// min1 - C * min0 + C * x
	// (min1 - C * min0) + C * x
	//         b         + a * x
	// a * x + b
	if (Math::is_equal_approx(max0, min0)) {
		out_a = 0;
		out_b = 0;
		return;
	}
	const float a = (max1 - min1) / (max0 - min0);
	const float b = min1 - a * min0;
	out_a = a;
	out_b = b;
}

} // namespace zylann::math

#endif // VOXEL_MATH_FUNCS_H
