#ifndef ZN_GODOT_RENDERING_SERVER_H
#define ZN_GODOT_RENDERING_SERVER_H

#if defined(ZN_GODOT)
#include <servers/rendering_server.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/rendering_server.hpp>
using namespace godot;
#endif

#include "../../containers/std_vector.h"

namespace zylann::godot {

inline void free_rendering_server_rid(RenderingServer &rs, const RID &rid) {
#if defined(ZN_GODOT)
	rs.free(rid);
#elif defined(ZN_GODOT_EXTENSION)
	rs.free_rid(rid);
#endif
}

struct ShaderParameterInfo {
	String name;
	Variant::Type type;
};

void get_shader_parameter_list(const RID &shader_rid, StdVector<ShaderParameterInfo> &out_parameters);

} // namespace zylann::godot

#endif // ZN_GODOT_RENDERING_SERVER_H
