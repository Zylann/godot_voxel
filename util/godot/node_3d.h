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

} // namespace zylann

#endif // ZN_GODOT_NODE_3D_H
