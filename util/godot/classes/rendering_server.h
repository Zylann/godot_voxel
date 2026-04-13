#ifndef ZN_GODOT_RENDERING_SERVER_H
#define ZN_GODOT_RENDERING_SERVER_H

#if defined(ZN_GODOT)
#include "../core/version.h"

#if GODOT_VERSION_MAJOR == 4 && GODOT_VERSION_MINOR <= 5
#include <servers/rendering_server.h>
#else
#include <servers/rendering/rendering_server.h>
#endif

#if GODOT_VERSION_MAJOR == 4 && GODOT_VERSION_MINOR <= 6
using RenderingServerEnums = RenderingServer;
#else
// RenderingServer enums are now in a dedicated namespace called RenderingServerEnums
#include <servers/rendering/rendering_server_enums.h>
#endif

#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/rendering_server.hpp>
using namespace godot;
using RenderingServerEnums = RenderingServer;
#endif

#include "../../containers/std_vector.h"
#include "../macros.h"

ZN_GODOT_FORWARD_DECLARE(class ProjectSettings);

namespace zylann::godot {

inline void free_rendering_server_rid(RenderingServer &rs, const RID &rid) {
#if defined(ZN_GODOT)
#if GODOT_VERSION_MAJOR == 4 && GODOT_VERSION_MINOR <= 5
	rs.free(rid);
#else
	rs.free_rid(rid);
#endif

#elif defined(ZN_GODOT_EXTENSION)
	rs.free_rid(rid);
#endif
}

struct ShaderParameterInfo {
	String name;
	Variant::Type type;
};

void get_shader_parameter_list(const RID &shader_rid, StdVector<ShaderParameterInfo> &out_parameters);

String get_current_rendering_method_name();

// Enum equivalent to strings used in ProjectSettings and RenderingServer.
enum RenderMethod {
	RENDER_METHOD_FORWARD_PLUS,
	RENDER_METHOD_MOBILE,
	RENDER_METHOD_GL_COMPATIBILITY,
	RENDER_METHOD_UNKNOWN,
};

RenderMethod get_current_rendering_method();

String get_current_rendering_driver_name();

enum RenderDriverName {
	RENDER_DRIVER_VULKAN,
	RENDER_DRIVER_D3D12,
	RENDER_DRIVER_METAL,
	RENDER_DRIVER_OPENGL3,
	RENDER_DRIVER_OPENGL3_ES,
	RENDER_DRIVER_OPENGL3_ANGLE,
	RENDER_DRIVER_UNKNOWN,
};

RenderDriverName get_current_rendering_driver();

// Enum equivalent of `ProjectSettings.rendering/driver/threads/thread_model`.
// This is unfortunately not exposed.
enum RenderThreadModel {
	RENDER_THREAD_UNSAFE,
	RENDER_THREAD_SAFE,
	RENDER_SEPARATE_THREAD,
};

RenderThreadModel get_render_thread_model(const ProjectSettings &settings);

// Tells if it is safe to call functions of the RenderingServer from a thread other than the main one.
bool is_render_thread_model_safe(const RenderThreadModel mode);

} // namespace zylann::godot

#endif // ZN_GODOT_RENDERING_SERVER_H
