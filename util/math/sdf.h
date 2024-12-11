#ifndef ZN_MATH_SDF_H
#define ZN_MATH_SDF_H

#include "interval.h"
#include "vector2t.h"
#include "vector3t.h"

namespace zylann::math {

// Signed-distance-field functions.
// For more, see https://www.iquilezles.org/www/articles/distfunctions/distfunctions.htm

// TODO Use `float`, or templatize SDF values. Doubles may prevent some SIMD optimizations.

template <typename T>
inline T sdf_box(const Vector3T<T> pos, const Vector3T<T> extents) {
	static_assert(std::is_floating_point<T>::value);
	const Vector3T<T> d = abs(pos) - extents;
	return min(max(d.x, max(d.y, d.z)), T(0)) + length(Vector3T<T>(max(d.x, T(0)), max(d.y, T(0)), max(d.z, T(0))));
}

template <typename T>
inline IntervalT<T> sdf_box(
		const IntervalT<T> &x,
		const IntervalT<T> &y,
		const IntervalT<T> &z,
		const IntervalT<T> &sx,
		const IntervalT<T> &sy,
		const IntervalT<T> &sz
) {
	const IntervalT<T> dx = abs(x) - sx;
	const IntervalT<T> dy = abs(y) - sy;
	const IntervalT<T> dz = abs(z) - sz;
	return min_interval(max_interval(dx, max_interval(dy, dz)), T(0)) +
			get_length(max_interval(dx, T(0)), max_interval(dy, T(0)), max_interval(dz, T(0)));
}

template <typename T>
inline T sdf_sphere(const Vector3T<T> pos, const Vector3T<T> center, const T radius) {
	return distance(pos, center) - radius;
}

template <typename T>
inline T sdf_torus(const T x, const T y, const T z, const T r0, const T r1) {
	const Vector2T<T> q = Vector2T<T>(length(Vector2T<T>(x, z)) - r0, y);
	return length(q) - r1;
}

template <typename T>
inline IntervalT<T> sdf_torus(
		const IntervalT<T> &x,
		const IntervalT<T> &y,
		const IntervalT<T> &z,
		const IntervalT<T> r0,
		const IntervalT<T> r1
) {
	const IntervalT<T> qx = get_length(x, z) - r0;
	return get_length(qx, y) - r1;
}

// Note: calculate `plane_d` as `dot(plane_normal, point_in_plane)`
template <typename T>
inline T sdf_plane(const Vector3T<T> pos, const Vector3T<T> plane_normal, const T plane_d) {
	// On Inigo's website it's a `+h`, but it seems to be backward because then if a plane has normal (0,1,0) and height
	// 1, a point at (0,1,0) will give a dot of 1, + height will be 2, which is wrong because the expected SDF here
	// would be 0.
	return dot(pos, plane_normal) - plane_d;
}

template <typename T>
inline T sdf_union(const T a, const T b) {
	return min(a, b);
}

// Subtracts SDF b from SDF a
template <typename T>
inline T sdf_subtract(const T a, const T b) {
	return max(a, -b);
}

template <typename T>
inline T sdf_smooth_union(const T a, const T b, const T s) {
	const T h = clamp(T(0.5) + T(0.5) * (b - a) / s, T(0), T(1));
	return lerp(b, a, h) - s * h * (T(1) - h);
}

// Inverted a and b because it subtracts SDF a from SDF b
template <typename T>
inline T sdf_smooth_subtract(T b, T a, T s) {
	const T h = clamp(T(0.5) - T(0.5) * (b + a) / s, T(0), T(1));
	return lerp(b, -a, h) + s * h * (T(1) - h);
}

template <typename T>
inline IntervalT<T> sdf_union(const IntervalT<T> a, const IntervalT<T> b) {
	return min_interval(a, b);
}

// Does a - b
template <typename T>
inline IntervalT<T> sdf_subtract(const IntervalT<T> a, const IntervalT<T> b) {
	return max_interval(a, -b);
}

template <typename T, typename F>
inline IntervalT<T> sdf_smooth_op(const IntervalT<T> b, const IntervalT<T> a, T s, F smooth_op_func) {
	// Smooth union and subtract are a generalization of `min(a, b)` and `max(-a, b)`, with a smooth junction.
	// That junction runs in a diagonal crossing zero (with equation `y = -x`).
	// Areas on the two sides of the junction are monotonic, i.e their derivatives should never cross zero,
	// because they are linear functions modified by a "smoothing" polynomial for which the tip is on diagonal.
	// So to find the output range, we can evaluate and sort the 4 corners,
	// and diagonal intersections if it crosses the area.

	//     |  \              |
	//  ---1---x-------------3--- b.max
	//     |    \            |
	//     |     \           |              (b)
	//     |      \          |               y
	//     |       \         |               |
	//  ---0--------x--------2--- b.min      o---x (a)
	//     |         \       |
	//    a.min             a.max

	const T v0 = smooth_op_func(b.min, a.min, s);
	const T v1 = smooth_op_func(b.max, a.min, s);
	const T v2 = smooth_op_func(b.min, a.max, s);
	const T v3 = smooth_op_func(b.max, a.max, s);

	const Vector2T<T> diag_b_min(-b.min, b.min);
	const Vector2T<T> diag_b_max(-b.max, b.max);
	const Vector2T<T> diag_a_min(a.min, -a.min);
	const Vector2T<T> diag_a_max(a.max, -a.max);

	const bool crossing_top = (diag_b_max.x > a.min && diag_b_max.x < a.max);
	const bool crossing_left = (diag_a_min.y > b.min && diag_a_min.y < b.max);

	if (crossing_left || crossing_top) {
		const bool crossing_right = (diag_a_max.y > b.min && diag_a_max.y < b.max);

		T v4;
		if (crossing_left) {
			v4 = smooth_op_func(diag_a_min.y, diag_a_min.x, s);
		} else {
			v4 = smooth_op_func(diag_b_max.y, diag_b_max.x, s);
		}

		T v5;
		if (crossing_right) {
			v5 = smooth_op_func(diag_a_max.y, diag_a_max.x, s);
		} else {
			v5 = smooth_op_func(diag_b_min.y, diag_b_min.x, s);
		}

		return IntervalT<T>(min(v0, v1, v2, v3, v4, v5), max(v0, v1, v2, v3, v4, v5));
	}

	// The diagonal does not cross the area
	return IntervalT<T>(min(v0, v1, v2, v3), max(v0, v1, v2, v3));
}

template <typename T>
IntervalT<T> sdf_smooth_union(const IntervalT<T> p_b, const IntervalT<T> p_a, const T p_s) {
	// TODO Not tested
	// Had to use a lambda because otherwise it's ambiguous
	return sdf_smooth_op(p_b, p_a, p_s, [](const T b, const T a, const T s) { //
		return zylann::math::sdf_smooth_union(b, a, s);
	});
}

// Does b - a
template <typename T>
IntervalT<T> sdf_smooth_subtract(const IntervalT<T> p_b, const IntervalT<T> p_a, const T p_s) {
	return sdf_smooth_op(p_b, p_a, p_s, [](const T b, const T a, const T s) { //
		return zylann::math::sdf_smooth_subtract(b, a, s);
	});
}

enum SdfAffectingArguments { //
	SDF_ONLY_A,
	SDF_ONLY_B,
	SDF_BOTH
};

// Tests which argument can affect the result.
// for a - b
template <typename T>
SdfAffectingArguments sdf_subtract_side(const IntervalT<T> a, const IntervalT<T> b) {
	if (b.min > -a.min) {
		return SDF_ONLY_A;
	}
	if (b.max < -a.max) {
		return SDF_ONLY_B;
	}
	return SDF_BOTH;
}

// for a - b
template <typename T>
SdfAffectingArguments sdf_polynomial_smooth_subtract_side(const IntervalT<T> a, const IntervalT<T> b, const T s) {
	//     |  \  \  \        |
	//  ---1---x--x--x-------3--- b.max
	//     |    \  \  \      |
	//     |     \  \  \     |              (b)
	//     |      \  \  \    |               y
	//     |       \  \  \   |               |
	//  ---0--------x--x--x--2--- b.min      o---x (a)
	//     |         \s \ s\ |
	//    a.min             a.max

	if (b.min > -a.min + s) {
		return SDF_ONLY_A;
	}
	if (b.max < -a.max - s) {
		return SDF_ONLY_B;
	}
	return SDF_BOTH;
}

template <typename T>
SdfAffectingArguments sdf_union_side(const IntervalT<T> a, const IntervalT<T> b) {
	if (a.max < b.min) {
		return SDF_ONLY_A;
	}
	if (b.max < a.min) {
		return SDF_ONLY_B;
	}
	return SDF_BOTH;
}

template <typename T>
SdfAffectingArguments sdf_polynomial_smooth_union_side(const IntervalT<T> a, const IntervalT<T> b, const T s) {
	if (a.max + s < b.min) {
		return SDF_ONLY_A;
	}
	if (b.max + s < a.min) {
		return SDF_ONLY_B;
	}
	return SDF_BOTH;
}

template <typename T>
inline T sdf_round_cone(const Vector3T<T> p, const Vector3T<T> a, const Vector3T<T> b, const T r1, const T r2) {
	// sampling independent computations (only depend on shape)
	const Vector3T<T> ba = b - a;
	const T l2 = dot(ba, ba);
	const T rr = r1 - r2;
	const T a2 = l2 - rr * rr;
	const T il2 = 1.0 / l2;

	// sampling dependant computations
	const Vector3T<T> pa = p - a;
	const T y = dot(pa, ba);
	const T z = y - l2;
	const T x2 = length_squared(pa * l2 - ba * y);
	const T y2 = y * y * l2;
	const T z2 = z * z * l2;

	// single square root!
	const T k = sign(rr) * rr * rr * x2;
	if (sign(z) * a2 * z2 > k) {
		return sqrt(x2 + z2) * il2 - r2;
	}
	if (sign(y) * a2 * y2 < k) {
		return sqrt(x2 + y2) * il2 - r1;
	}
	return (sqrt(x2 * a2 * il2) + y * rr) * il2 - r1;
}

template <typename T>
struct SdfRoundConePrecalc {
	Vector3T<T> a;
	Vector3T<T> b;
	T r1;
	T r2;

	Vector3T<T> ba;
	T l2;
	T rr;
	T a2;
	T il2;

	void update() {
		ba = b - a;
		l2 = dot(ba, ba);
		rr = r1 - r2;
		a2 = l2 - rr * rr;
		il2 = T(1) / l2;
	}

	T operator()(Vector3T<T> p) const {
		const Vector3T<T> pa = p - a;
		const T y = dot(pa, ba);
		const T z = y - l2;
		const T x2 = length_squared(pa * l2 - ba * y);
		const T y2 = y * y * l2;
		const T z2 = z * z * l2;

		const T k = sign(rr) * rr * rr * x2;
		if (sign(z) * a2 * z2 > k) {
			return sqrt(x2 + z2) * il2 - r2;
		}
		if (sign(y) * a2 * y2 < k) {
			return sqrt(x2 + y2) * il2 - r1;
		}
		return (sqrt(x2 * a2 * il2) + y * rr) * il2 - r1;
	}
};

} // namespace zylann::math

#endif // ZN_MATH_SDF_H
