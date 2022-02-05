#ifndef OSN_NOISE_RANGE_H
#define OSN_NOISE_RANGE_H

#include "../math/interval.h"

// Interval estimation for Godot's OpenSimplexNoise

class OpenSimplexNoise;

namespace zylann {

math::Interval get_osn_range_2d(OpenSimplexNoise *noise, math::Interval x, math::Interval y);
math::Interval get_osn_range_3d(OpenSimplexNoise *noise, math::Interval x, math::Interval y, math::Interval z);

} // namespace zylann

#endif // OSN_NOISE_RANGE_H
