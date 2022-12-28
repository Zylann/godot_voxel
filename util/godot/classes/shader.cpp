#include "shader.h"
#include "rendering_server.h"

namespace zylann {

#ifdef TOOLS_ENABLED

// TODO Cannot use `Shader.has_uniform()` because it is unreliable.
// See https://github.com/godotengine/godot/issues/64467
bool shader_has_uniform(const Shader &shader, StringName uniform_name) {
	std::vector<GodotShaderParameterInfo> params;
	get_shader_parameter_list(shader.get_rid(), params);
	for (const GodotShaderParameterInfo &pi : params) {
		if (pi.name == uniform_name) {
			return true;
		}
	}
	// List<PropertyInfo> params;
	// RenderingServer::get_singleton()->get_shader_parameter_list(shader.get_rid(), &params);
	// for (const PropertyInfo &pi : params) {
	// 	if (pi.name == uniform_name) {
	// 		return true;
	// 	}
	// }
	return false;
}

String get_missing_uniform_names(Span<const StringName> expected_uniforms, const Shader &shader) {
	String missing_uniforms;

	// TODO Cannot use `Shader.has_uniform()` because it is unreliable.
	// See https://github.com/godotengine/godot/issues/64467
	// for (unsigned int i = 0; i < expected_uniforms.size(); ++i) {
	// 	StringName uniform_name = expected_uniforms[i];
	// 	ZN_ASSERT_CONTINUE(uniform_name != StringName());
	// 	if (!shader.has_uniform(uniform_name)) {
	// 		if (missing_uniforms.size() > 0) {
	// 			missing_uniforms += ", ";
	// 		}
	// 		missing_uniforms += uniform_name;
	// 	}
	// }

	std::vector<GodotShaderParameterInfo> params;
	get_shader_parameter_list(shader.get_rid(), params);

	for (const StringName &name : expected_uniforms) {
		bool found = false;
		for (const GodotShaderParameterInfo &pi : params) {
			if (pi.name == name) {
				found = true;
				break;
			}
		}

		if (!found) {
			if (missing_uniforms.length() > 0) {
				// TODO GDX: `String::operator+=` is missing
				missing_uniforms = missing_uniforms + ", ";
			}
			// TODO GDX: `String::operator+=` is missing
			missing_uniforms = missing_uniforms + name;
		}
	}

	return missing_uniforms;
}

#endif

} // namespace zylann
