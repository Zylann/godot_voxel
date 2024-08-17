#ifndef ZN_GODOT_GEOMETRY_INSTANCE_3D_H
#define ZN_GODOT_GEOMETRY_INSTANCE_3D_H

#if defined(ZN_GODOT)
#include <scene/3d/visual_instance_3d.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/geometry_instance3d.hpp>
using namespace godot;
#endif

namespace zylann::godot {

void set_geometry_instance_gi_mode(RID rid, GeometryInstance3D::GIMode mode);

static constexpr int GI_MODE_COUNT = 3;

extern const char *const CAST_SHADOW_ENUM_HINT_STRING;
extern const char *const GI_MODE_ENUM_HINT_STRING;

} // namespace zylann::godot

#endif // ZN_GODOT_GEOMETRY_INSTANCE_3D_H
