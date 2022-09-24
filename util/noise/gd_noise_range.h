#ifndef VOXEL_GD_NOISE_RANGE_H
#define VOXEL_GD_NOISE_RANGE_H

#include "../macros.h"
#include "../math/interval.h"

ZN_GODOT_FORWARD_DECLARE(class Noise)

namespace zylann {

math::Interval get_range_2d(const Noise &noise, math::Interval x, math::Interval y);
math::Interval get_range_3d(const Noise &noise, math::Interval x, math::Interval y, math::Interval z);

} // namespace zylann

#endif // VOXEL_GD_NOISE_RANGE_H
