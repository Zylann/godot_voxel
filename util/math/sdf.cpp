#include "sdf.h"

namespace zylann::math {

SdfAffectingArguments sdf_subtract_side(Interval a, Interval b) {
	if (b.min > -a.min) {
		return SDF_ONLY_A;
	}
	if (b.max < -a.max) {
		return SDF_ONLY_B;
	}
	return SDF_BOTH;
}

SdfAffectingArguments sdf_polynomial_smooth_subtract_side(Interval a, Interval b, float s) {
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
		return SDF_ONLY_B;
	}
	if (b.max < -a.max - s) {
		return SDF_ONLY_A;
	}
	return SDF_BOTH;
}

SdfAffectingArguments sdf_union_side(Interval a, Interval b) {
	if (a.max < b.min) {
		return SDF_ONLY_A;
	}
	if (b.max < a.min) {
		return SDF_ONLY_B;
	}
	return SDF_BOTH;
}

SdfAffectingArguments sdf_polynomial_smooth_union_side(Interval a, Interval b, float s) {
	if (a.max + s < b.min) {
		return SDF_ONLY_A;
	}
	if (b.max + s < a.min) {
		return SDF_ONLY_B;
	}
	return SDF_BOTH;
}

template <typename F>
inline Interval sdf_smooth_op(Interval b, Interval a, float s, F smooth_op_func) {
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

	const float v0 = smooth_op_func(b.min, a.min, s);
	const float v1 = smooth_op_func(b.max, a.min, s);
	const float v2 = smooth_op_func(b.min, a.max, s);
	const float v3 = smooth_op_func(b.max, a.max, s);

	const Vector2 diag_b_min(-b.min, b.min);
	const Vector2 diag_b_max(-b.max, b.max);
	const Vector2 diag_a_min(a.min, -a.min);
	const Vector2 diag_a_max(a.max, -a.max);

	const bool crossing_top = (diag_b_max.x > a.min && diag_b_max.x < a.max);
	const bool crossing_left = (diag_a_min.y > b.min && diag_a_min.y < b.max);

	if (crossing_left || crossing_top) {
		const bool crossing_right = (diag_a_max.y > b.min && diag_a_max.y < b.max);

		float v4;
		if (crossing_left) {
			v4 = smooth_op_func(diag_a_min.y, diag_a_min.x, s);
		} else {
			v4 = smooth_op_func(diag_b_max.y, diag_b_max.x, s);
		}

		float v5;
		if (crossing_right) {
			v5 = smooth_op_func(diag_a_max.y, diag_a_max.x, s);
		} else {
			v5 = smooth_op_func(diag_b_min.y, diag_b_min.x, s);
		}

		return Interval(min(v0, v1, v2, v3, v4, v5), max(v0, v1, v2, v3, v4, v5));
	}

	// The diagonal does not cross the area
	return Interval(min(v0, v1, v2, v3), max(v0, v1, v2, v3));
}

Interval sdf_smooth_union(Interval p_b, Interval p_a, float p_s) {
	// TODO Not tested
	// Had to use a lambda because otherwise it's ambiguous
	return sdf_smooth_op(
			p_b, p_a, p_s, [](float b, float a, float s) { return zylann::math::sdf_smooth_union(b, a, s); });
}

Interval sdf_smooth_subtract(Interval p_b, Interval p_a, float p_s) {
	return sdf_smooth_op(
			p_b, p_a, p_s, [](float b, float a, float s) { return zylann::math::sdf_smooth_subtract(b, a, s); });
}

} //namespace zylann::math
