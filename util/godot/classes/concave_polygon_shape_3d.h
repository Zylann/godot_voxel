#ifndef ZN_GODOT_CONCAVE_POLYGON_SHAPE_3D_H
#define ZN_GODOT_CONCAVE_POLYGON_SHAPE_3D_H

#include "../../macros.h"
#include "../../math/vector3f.h"
#include "../../span.h"

#if defined(ZN_GODOT)
#include <scene/resources/concave_polygon_shape_3d.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/concave_polygon_shape3d.hpp>
using namespace godot;
#endif

namespace zylann {

// Combines all mesh surface arrays into one collider.
Ref<ConcavePolygonShape3D> create_concave_polygon_shape(Span<const Array> surfaces);
Ref<ConcavePolygonShape3D> create_concave_polygon_shape(Span<const Vector3f> positions, Span<const int> indices);
// Create shape from a sub-region of a mesh surface (starting at 0).
Ref<ConcavePolygonShape3D> create_concave_polygon_shape(const Array surface_arrays, unsigned int index_count);

} // namespace zylann

#endif // ZN_GODOT_CONCAVE_POLYGON_SHAPE_3D_H
