#ifndef ZN_GODOT_PHYSICS_SERVER_3D_H
#define ZN_GODOT_PHYSICS_SERVER_3D_H

#if defined(ZN_GODOT)
#include <servers/physics_server_3d.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/physics_server3d.hpp>
using namespace godot;
#endif

namespace zylann {

inline void free_physics_server_rid(PhysicsServer3D &ps, const RID &rid) {
#if defined(ZN_GODOT)
	ps.free(rid);
#elif defined(ZN_GODOT_EXTENSION)
	ps.free_rid(rid);
#endif
}

} // namespace zylann

#endif // ZN_GODOT_PHYSICS_SERVER_3D_H
