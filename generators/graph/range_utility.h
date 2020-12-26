#ifndef RANGE_UTILITY_H
#define RANGE_UTILITY_H

#include "../../math/interval.h"

class Curve;
class OpenSimplexNoise;
class Image;

Interval get_osn_range_2d(OpenSimplexNoise *noise, Interval x, Interval y);
Interval get_osn_range_3d(OpenSimplexNoise *noise, Interval x, Interval y, Interval z);

Interval get_curve_range(Curve &curve, uint8_t &is_monotonic_increasing);

Interval get_heightmap_range(Image &im);
Interval get_heightmap_range(Image &im, Rect2i rect);

#ifdef DEBUG_ENABLED
void test_osn_max_derivative();
#endif

#endif // RANGE_UTILITY_H
