#ifndef ZN_GODOT_SHADER_H
#define ZN_GODOT_SHADER_H

#include "../span.h"
#include <core/string/string_name.h>

class Shader;

namespace zylann {

#ifdef TOOLS_ENABLED

// TODO Cannot use `Shader.has_uniform()` because it is unreliable.
// See https://github.com/godotengine/godot/issues/64467
bool shader_has_uniform(const Shader &shader, StringName uniform_name);

String get_missing_uniform_names(Span<const StringName> expected_uniforms, const Shader &shader);

#endif

} // namespace zylann

#endif // ZN_GODOT_SHADER_H
