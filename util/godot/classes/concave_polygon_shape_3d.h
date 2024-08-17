#ifndef ZN_GODOT_CONCAVE_POLYGON_SHAPE_3D_H
#define ZN_GODOT_CONCAVE_POLYGON_SHAPE_3D_H

#include "../../containers/span.h"
#include "../../macros.h"
#include "../../math/vector3f.h"

#if defined(ZN_GODOT)
#include <core/version.h>

#if VERSION_MAJOR == 4 && VERSION_MINOR <= 2
#include <scene/resources/concave_polygon_shape_3d.h>
#else
#include <scene/resources/3d/concave_polygon_shape_3d.h>
#endif

#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/concave_polygon_shape3d.hpp>
using namespace godot;
#endif

namespace zylann::godot {

// Combines all mesh surface arrays into one collider.
Ref<ConcavePolygonShape3D> create_concave_polygon_shape(Span<const Array> surfaces);
Ref<ConcavePolygonShape3D> create_concave_polygon_shape(Span<const Vector3f> positions, Span<const int> indices);
// Create shape from a sub-region of a mesh surface (starting at 0).
Ref<ConcavePolygonShape3D> create_concave_polygon_shape(const Array surface_arrays, unsigned int index_count);

} // namespace zylann::godot

#endif // ZN_GODOT_CONCAVE_POLYGON_SHAPE_3D_H
