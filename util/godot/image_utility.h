#ifndef ZN_GODOT_IMAGE_UTILITY_H
#define ZN_GODOT_IMAGE_UTILITY_H

#include "../math/interval.h"
#include "classes/image.h"
#include <array>

namespace zylann::godot {
namespace ImageUtility {

std::array<math::Interval, 4> get_range(const Image &im);
std::array<math::Interval, 4> get_range(const Image &im, const Rect2i rect);

} // namespace ImageUtility
} // namespace zylann::godot

#endif // ZN_GODOT_IMAGE_UTILITY_H
