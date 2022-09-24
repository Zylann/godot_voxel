#ifndef ZN_GODOT_MULTIMESH_H
#define ZN_GODOT_MULTIMESH_H

#if defined(ZN_GODOT)
#include <scene/resources/multimesh.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/multi_mesh.hpp>
using namespace godot;
#endif

namespace zylann {

// This API can be confusing so I made a wrapper
int get_visible_instance_count(const MultiMesh &mm);

} // namespace zylann

#endif // ZN_GODOT_MULTIMESH_H
