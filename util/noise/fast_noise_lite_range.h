#ifndef FAST_NOISE_LITE_RANGE_H
#define FAST_NOISE_LITE_RANGE_H

#include "../math/interval.h"

// Interval estimation for FastNoiseLite

namespace zylann {

class FastNoiseLite;
class FastNoiseLiteGradient;

math::Interval get_fnl_range_2d(const FastNoiseLite *noise, math::Interval x, math::Interval y);
math::Interval get_fnl_range_3d(const FastNoiseLite *noise, math::Interval x, math::Interval y, math::Interval z);
math::Interval2 get_fnl_gradient_range_2d(const FastNoiseLiteGradient *noise, math::Interval x, math::Interval y);
math::Interval3 get_fnl_gradient_range_3d(
		const FastNoiseLiteGradient *noise, math::Interval x, math::Interval y, math::Interval z);

} // namespace zylann

#endif // FAST_NOISE_LITE_RANGE_H
