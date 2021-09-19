#ifndef RANGE_UTILITY_H
#define RANGE_UTILITY_H

#include "../../util/math/interval.h"
#include <core/math/rect2.h>
#include <vector>

class Curve;
class OpenSimplexNoise;
class Image;
class FastNoiseLite;
class FastNoiseLiteGradient;

Interval get_osn_range_2d(OpenSimplexNoise *noise, Interval x, Interval y);
Interval get_osn_range_3d(OpenSimplexNoise *noise, Interval x, Interval y, Interval z);

struct CurveMonotonicSection {
	float x_min;
	float x_max;
	// Note: Y values are not necessarily in increasing order.
	// Their name only means to correspond to X coordinates.
	float y_min;
	float y_max;
};

struct CurveRangeData {
	std::vector<CurveMonotonicSection> sections;
};

static const float CURVE_RANGE_MARGIN = CMP_EPSILON;

// Gathers monotonic sections of a curve, at baked resolution.
// Within one section, the curve has only one of the following properties:
// - Be stationary or decrease
// - Be stationary or increase
// Which means, within one section, given a range of input values defined by a min and max,
// we can quickly calculate an accurate range of output values by sampling the curve only at the two points.
void get_curve_monotonic_sections(Curve &curve, std::vector<CurveMonotonicSection> &sections);
// Gets the range of Y values for a range of X values on a curve, using precalculated monotonic segments
Interval get_curve_range(Curve &curve, const std::vector<CurveMonotonicSection> &sections, Interval x);

// Legacy
Interval get_curve_range(Curve &curve, bool &is_monotonic_increasing);

Interval get_heightmap_range(Image &im);
Interval get_heightmap_range(Image &im, Rect2i rect);

inline Interval sdf_union(Interval a, Interval b) {
	return min_interval(a, b);
}

// Does a - b
inline Interval sdf_subtract(Interval a, Interval b) {
	return max_interval(a, -b);
}

Interval sdf_smooth_union(Interval p_b, Interval p_a, float p_s);

// Does b - a
Interval sdf_smooth_subtract(Interval p_b, Interval p_a, float p_s);

enum SdfAffectingArguments {
	SDF_ONLY_A,
	SDF_ONLY_B,
	SDF_BOTH
};

// Tests which argument can affect the result.
// for a - b
SdfAffectingArguments sdf_subtract_side(Interval a, Interval b);
// for a - b
SdfAffectingArguments sdf_polynomial_smooth_subtract_side(Interval a, Interval b, float s);

SdfAffectingArguments sdf_union_side(Interval a, Interval b);
SdfAffectingArguments sdf_polynomial_smooth_union_side(Interval a, Interval b, float s);

struct Interval2 {
	Interval x;
	Interval y;
};

struct Interval3 {
	Interval x;
	Interval y;
	Interval z;
};

Interval get_fnl_range_2d(const FastNoiseLite *noise, Interval x, Interval y);
Interval get_fnl_range_3d(const FastNoiseLite *noise, Interval x, Interval y, Interval z);
Interval2 get_fnl_gradient_range_2d(const FastNoiseLiteGradient *noise, Interval x, Interval y);
Interval3 get_fnl_gradient_range_3d(const FastNoiseLiteGradient *noise, Interval x, Interval y, Interval z);

#endif // RANGE_UTILITY_H
