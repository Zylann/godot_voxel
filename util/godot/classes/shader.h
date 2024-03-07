#ifndef ZN_GODOT_SHADER_H
#define ZN_GODOT_SHADER_H

#if defined(ZN_GODOT)
#include <scene/resources/shader.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/shader.hpp>
using namespace godot;
#endif

#ifdef TOOLS_ENABLED

#include "../../containers/span.h"

namespace zylann::godot {

// TODO Cannot use `Shader.has_uniform()` because it is unreliable.
// See https://github.com/godotengine/godot/issues/64467
bool shader_has_uniform(const Shader &shader, StringName uniform_name);

String get_missing_uniform_names(Span<const StringName> expected_uniforms, const Shader &shader);

} // namespace zylann::godot

#endif

#endif // ZN_GODOT_SHADER_H
