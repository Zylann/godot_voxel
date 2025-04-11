#include "rendering_server.h"
#include "../../errors.h"
#include "../../string/format.h"
#include "../core/string.h"
#include "../core/version.h"
#include "project_settings.h"

namespace zylann::godot {

void get_shader_parameter_list(const RID &shader_rid, StdVector<ShaderParameterInfo> &out_parameters) {
#if defined(ZN_GODOT)
	List<PropertyInfo> params;
	RenderingServer::get_singleton()->get_shader_parameter_list(shader_rid, &params);
	// I'd like to use ConstIterator since I only read that list but that isn't possible :shrug:
	for (List<PropertyInfo>::Iterator it = params.begin(); it != params.end(); ++it) {
		const PropertyInfo property = *it;
		ShaderParameterInfo pi;
		pi.type = property.type;
		pi.name = property.name;
		out_parameters.push_back(pi);
	}

#elif defined(ZN_GODOT_EXTENSION)
	const Array properties = RenderingServer::get_singleton()->get_shader_parameter_list(shader_rid);
	const String type_key = "type";
	const String name_key = "name";
	for (int i = 0; i < properties.size(); ++i) {
		Dictionary d = properties[i];
		ShaderParameterInfo pi;
		pi.type = Variant::Type(int(d[type_key]));
		pi.name = d[name_key];
		out_parameters.push_back(pi);
	}
#endif
}

String get_current_rendering_method_name() {
#if GODOT_VERSION_MAJOR == 4 && GODOT_VERSION_MINOR >= 4
	RenderingServer *rs = RenderingServer::get_singleton();
	// RenderingServer can be null with `tests=yes`.
	ZN_ASSERT_RETURN_V(rs != nullptr, "");

	const String method_name = rs->get_current_rendering_method();
	return method_name;

#else
	// See https://github.com/godotengine/godot/pull/85430

#if defined(ZN_GODOT)
	OS *os = OS::get_singleton();
	ZN_ASSERT_RETURN_V(os != nullptr, "");
	return os->get_current_rendering_method();

#elif defined(ZN_GODOT_EXTENSION)
	ZN_PRINT_WARNING("Unable to get current rendering method, Godot doesn't expose it.");
	return "";
#endif

#endif
}

RenderMethod get_current_rendering_method() {
	const String name = get_current_rendering_method_name();

	if (name == "forward_plus") {
		return RENDER_METHOD_FORWARD_PLUS;
	}
	if (name == "mobile") {
		return RENDER_METHOD_MOBILE;
	}
	if (name == "gl_compatibility") {
		return RENDER_METHOD_GL_COMPATIBILITY;
	}

	ZN_PRINT_WARNING(format("Rendering method {} is unknown", name));
	return RENDER_METHOD_UNKNOWN;
}

String get_current_rendering_driver_name() {
#if GODOT_VERSION_MAJOR == 4 && GODOT_VERSION_MINOR >= 4
	RenderingServer *rs = RenderingServer::get_singleton();
	// RenderingServer can be null with `tests=yes`.
	ZN_ASSERT_RETURN_V(rs != nullptr, "");

	const String driver_name = rs->get_current_rendering_driver_name();
	return driver_name;

#else
	// See https://github.com/godotengine/godot/pull/85430

#if defined(ZN_GODOT)
	OS *os = OS::get_singleton();
	ZN_ASSERT_RETURN_V(os != nullptr, "");
	return os->get_current_rendering_driver_name();

#elif defined(ZN_GODOT_EXTENSION)
	ZN_PRINT_WARNING("Unable to get current rendering driver name, Godot doesn't expose it.");
	return "";
#endif

#endif
}

RenderDriverName get_current_rendering_driver() {
	const String name = get_current_rendering_driver_name();

	if (name == "vulkan") {
		return RENDER_DRIVER_VULKAN;
	}
	if (name == "d3d12") {
		return RENDER_DRIVER_D3D12;
	}
	if (name == "metal") {
		return RENDER_DRIVER_METAL;
	}
	if (name == "opengl3") {
		return RENDER_DRIVER_OPENGL3;
	}
	if (name == "opengl3_es") {
		return RENDER_DRIVER_OPENGL3_ES;
	}
	if (name == "opengl3_angle") {
		return RENDER_DRIVER_OPENGL3_ANGLE;
	}

	ZN_PRINT_WARNING(format("Rendering driver {} is unknown", name));
	return RENDER_DRIVER_UNKNOWN;
}

RenderThreadModel get_render_thread_model(const ProjectSettings &settings) {
	const int rendering_thread_model = settings.get("rendering/driver/threads/thread_model");
	return static_cast<RenderThreadModel>(rendering_thread_model);
}

bool is_render_thread_model_safe(const RenderThreadModel mode) {
	switch (mode) {
		case RENDER_SEPARATE_THREAD:
		case RENDER_THREAD_SAFE:
			return true;
		case RENDER_THREAD_UNSAFE:
			return false;
		default:
			ZN_PRINT_ERROR("Unhandled enum value");
			return false;
	}
}

} // namespace zylann::godot
