#ifndef ZN_GODOT_COLLISION_SHAPE_H
#define ZN_GODOT_COLLISION_SHAPE_H

#include "../math/vector3f.h"
#include "../span.h"
#include <core/object/ref_counted.h>

class ConcavePolygonShape3D;

namespace zylann {

// Combines all mesh surface arrays into one collider.
Ref<ConcavePolygonShape3D> create_concave_polygon_shape(Span<const Array> surfaces);
Ref<ConcavePolygonShape3D> create_concave_polygon_shape(Span<const Vector3f> positions, Span<const int> indices);
// Create shape from a sub-region of a mesh surface (starting at 0).
Ref<ConcavePolygonShape3D> create_concave_polygon_shape(const Array surface_arrays, unsigned int index_count);

} // namespace zylann

#endif // ZN_GODOT_COLLISION_SHAPE_H
