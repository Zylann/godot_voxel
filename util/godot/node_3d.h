#ifndef ZN_GODOT_NODE_3D_H
#define ZN_GODOT_NODE_3D_H

#if defined(ZN_GODOT)
#include <scene/3d/node_3d.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/node3d.hpp>
using namespace godot;
#endif

namespace zylann {

// TODO GDX: `NOTIFICATION_LOCAL_TRANSFORM_CHANGED` is missing from the Godot API.
#ifdef ZN_GODOT_EXTENSION
static const int NOTIFICATION_LOCAL_TRANSFORM_CHANGED = 44;
#else
static const int NOTIFICATION_LOCAL_TRANSFORM_CHANGED = Node3D::NOTIFICATION_LOCAL_TRANSFORM_CHANGED;
#endif // ZN_GODOT_EXTENSION

// TODO GDX: `NOTIFICATION_*` (and maybe other constants) are not static, they are supposed to be! Can't be use them in
// `switch`, and they take space in every class instance.
namespace godot_cpp_fix {
#ifdef ZN_GODOT_EXTENSION
#define ZN_GODOT_NODE_3D_CONSTANT(m_name) godot_cpp_fix::m_name
static const int NOTIFICATION_TRANSFORM_CHANGED = 2000;
static const int NOTIFICATION_ENTER_WORLD = 41;
static const int NOTIFICATION_EXIT_WORLD = 42;
static const int NOTIFICATION_VISIBILITY_CHANGED = 43;
#else
#define ZN_GODOT_NODE_CONSTANT(m_name) m_name
#endif // ZN_GODOT_EXTENSION
} // namespace godot_cpp_fix

} // namespace zylann

#endif // ZN_GODOT_NODE_3D_H
