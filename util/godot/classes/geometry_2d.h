#ifndef ZN_GODOT_GEOMETRY_2D_H
#define ZN_GODOT_GEOMETRY_2D_H

#if defined(ZN_GODOT)
#include <core/math/geometry_2d.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/geometry2d.hpp>
using namespace godot;
#endif

#include "../../math/vector2i.h"
#include "../../span.h"
#include <vector>

namespace zylann {

void geometry_2d_make_atlas(Span<const Vector2i> p_sizes, std::vector<Vector2i> &r_result, Vector2i &r_size);

} // namespace zylann

#endif // ZN_GODOT_GEOMETRY_2D_H
