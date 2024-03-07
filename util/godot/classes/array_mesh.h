#ifndef ZN_GODOT_ARRAY_MESH_H
#define ZN_GODOT_ARRAY_MESH_H

#if defined(ZN_GODOT)
#include <scene/resources/mesh.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/array_mesh.hpp>
using namespace godot;
#endif

namespace zylann::godot {

// TODO The following functions should be able to work on `Mesh`,
// but the script/extension API exposes some methods only on `ArrayMesh`, even though they exist on `Mesh` internally...

// TODO I need a cheap way to check this at `Mesh` level, but it seems it would require getting the surface arrays,
// which might not be cheap...
inline bool is_mesh_empty(const ArrayMesh &mesh) {
	if (mesh.get_surface_count() == 0) {
		return true;
	}
	if (mesh.surface_get_array_len(0) == 0) {
		return true;
	}
	return false;
}

#ifdef TOOLS_ENABLED

// Generates a wireframe-mesh that highlights edges of a triangle-mesh where vertices are not shared.
// Used for debugging.
Array generate_debug_seams_wireframe_surface(const ArrayMesh &src_mesh, int surface_index);

#endif

} // namespace zylann::godot

#endif // ZN_GODOT_ARRAY_MESH_H
