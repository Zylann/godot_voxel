#ifndef VOXEL_MATH_FUNCS_H
#define VOXEL_MATH_FUNCS_H

#include "../errors.h"
#include <core/math/math_funcs.h>

namespace zylann::math {

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
// float versions do not require casting all the time, so optional double-precision support with `real_t` is easier.

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

// Performs euclidean division, aka floored division.
// This implementation expects a strictly positive divisor.
//
// Example with division by 3:
//
//    x   | `/` | floordiv
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

inline int ceildiv(int x, int d) {
	return -floordiv(-x, d);
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

inline float sign(float x) {
	return x < 0.f ? -1.f : 1.f;
}

} // namespace zylann::math

#endif // VOXEL_MATH_FUNCS_H
