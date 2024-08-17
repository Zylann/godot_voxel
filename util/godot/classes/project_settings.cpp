#include "project_settings.h"

namespace zylann::godot {

void add_custom_project_setting(
		Variant::Type type,
		const char *name,
		PropertyHint hint,
		const char *hint_string,
		Variant default_value,
		bool requires_restart
) {
#if defined(ZN_GODOT)
	if (requires_restart) {
		GLOBAL_DEF_RST(name, default_value);
	} else {
		GLOBAL_DEF(name, default_value);
	}
	ProjectSettings::get_singleton()->set_custom_property_info(PropertyInfo(type, name, hint, hint_string));

#elif defined(ZN_GODOT_EXTENSION)
	ProjectSettings &ps = *ProjectSettings::get_singleton();

	String name_str(name);

	if (!ps.has_setting(name_str)) {
		ps.set_setting(name_str, default_value);
	}

	Dictionary d;
	d["name"] = name_str;
	d["type"] = type;
	d["hint"] = hint;
	d["hint_string"] = String(hint_string);
	ps.add_property_info(d);
	ps.set_initial_value(name, default_value);
	ps.set_restart_if_changed(name, requires_restart);
#endif
}

} // namespace zylann::godot
