#ifndef ZN_GODOT_EDITOR_SETTINGS_H
#define ZN_GODOT_EDITOR_SETTINGS_H

#if defined(ZN_GODOT)
#include <editor/editor_settings.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/editor_settings.hpp>
using namespace godot;
#endif

#include "shortcut.h"

namespace zylann {

Ref<Shortcut> get_or_create_editor_shortcut(const String &p_path, const String &p_name, Key p_keycode);

} // namespace zylann

#endif // ZN_GODOT_EDITOR_SETTINGS_H
