#ifndef ZN_GODOT_MESH_H
#define ZN_GODOT_MESH_H

#include "../span.h"
#include <core/variant/array.h>

class Mesh;

namespace zylann {

bool is_surface_triangulated(const Array &surface);
bool is_mesh_empty(const Mesh &mesh);
bool is_mesh_empty(Span<const Array> surfaces);

// Generates a wireframe-mesh that highlights edges of a triangle-mesh where vertices are not shared
Array generate_debug_seams_wireframe_surface(const Mesh &src_mesh, int surface_index);

} // namespace zylann

#endif // ZN_GODOT_MESH_H
