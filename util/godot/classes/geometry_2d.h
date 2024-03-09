#ifndef ZN_GODOT_GEOMETRY_2D_H
#define ZN_GODOT_GEOMETRY_2D_H

#if defined(ZN_GODOT)
#include <core/math/geometry_2d.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/geometry2d.hpp>
using namespace godot;
#endif

#include "../../containers/span.h"
#include "../../containers/std_vector.h"
#include "../../math/vector2i.h"

namespace zylann::godot {

void geometry_2d_make_atlas(Span<const Vector2i> p_sizes, StdVector<Vector2i> &r_result, Vector2i &r_size);

} // namespace zylann::godot

#endif // ZN_GODOT_GEOMETRY_2D_H
