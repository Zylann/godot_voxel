#ifndef ZN_GODOT_PROJECT_SETTINGS_H
#define ZN_GODOT_PROJECT_SETTINGS_H

#if defined(ZN_GODOT)
#include <core/config/project_settings.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/project_settings.hpp>
using namespace godot;
#endif

namespace zylann {

void add_custom_godot_project_setting(Variant::Type type, const char *name, PropertyHint hint, const char *hint_string,
		Variant default_value, bool requires_restart);

} // namespace zylann

#endif // ZN_GODOT_PROJECT_SETTINGS_H
