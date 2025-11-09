#ifndef ZN_GODOT_PHYSICS_SERVER_3D_H
#define ZN_GODOT_PHYSICS_SERVER_3D_H

#if defined(ZN_GODOT)
#include "../core/version.h"

#if GODOT_VERSION_MAJOR == 4 && GODOT_VERSION_MINOR <= 5
#include <servers/physics_server_3d.h>
#else
#include <servers/physics_3d/physics_server_3d.h>
#endif

#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/physics_server3d.hpp>
using namespace godot;
#endif

namespace zylann::godot {

inline void free_physics_server_rid(PhysicsServer3D &ps, const RID &rid) {
#if defined(ZN_GODOT)
#if GODOT_VERSION_MAJOR == 4 && GODOT_VERSION_MINOR <= 5
	ps.free(rid);
#else
	ps.free_rid(rid);
#endif

#elif defined(ZN_GODOT_EXTENSION)
	ps.free_rid(rid);
#endif
}

} // namespace zylann::godot

#endif // ZN_GODOT_PHYSICS_SERVER_3D_H
