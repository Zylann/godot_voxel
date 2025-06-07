#ifndef ZN_HEIGHTMAP_UTILITY_H
#define ZN_HEIGHTMAP_UTILITY_H

#include "../../util/godot/core/rect2i.h"
#include "../../util/godot/macros.h"
#include "../../util/math/interval.h"

ZN_GODOT_FORWARD_DECLARE(class Image);

namespace zylann {

math::Interval get_heightmap_range(const Image &im);
math::Interval get_heightmap_range(const Image &im, Rect2i rect);

} // namespace zylann

#endif // ZN_HEIGHTMAP_UTILITY_H
