#ifndef FAST_NOISE_LITE_RANGE_H
#define FAST_NOISE_LITE_RANGE_H

#include "../../math/interval.h"

// Interval estimation for ZN_FastNoiseLite

namespace zylann {

class ZN_FastNoiseLite;
class ZN_FastNoiseLiteGradient;

math::Interval get_fnl_range_2d(const ZN_FastNoiseLite &noise, math::Interval x, math::Interval y);
math::Interval get_fnl_range_3d(const ZN_FastNoiseLite &noise, math::Interval x, math::Interval y, math::Interval z);
math::Interval2 get_fnl_gradient_range_2d(const ZN_FastNoiseLiteGradient &noise, math::Interval x, math::Interval y);
math::Interval3 get_fnl_gradient_range_3d(
		const ZN_FastNoiseLiteGradient &noise, math::Interval x, math::Interval y, math::Interval z);

} // namespace zylann

#endif // FAST_NOISE_LITE_RANGE_H
