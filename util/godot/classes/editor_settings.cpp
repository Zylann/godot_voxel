#include "editor_settings.h"
#include "input_event_key.h"
#if defined(ZN_GODOT_EXTENSION)
#include "../core/keyboard.h"
#endif

namespace zylann::godot {

Ref<Shortcut> get_or_create_editor_shortcut(const String &p_path, const String &p_name, Key p_keycode) {
#if defined(ZN_GODOT)
	return ED_SHORTCUT(p_path, p_name, p_keycode);

#elif defined(ZN_GODOT_EXTENSION)
	// TODO GDX: `ED_SHORTCUT` and other `EditorSettings` shortcut APIs are not exposed
	// See https://github.com/godotengine/godot/pull/58585

	// Ported from core `ED_SHORTCUT_ARRAY`.
	// Returning a shortcut from the provided key without any registration to `EditorSettings`.
	// That means the shortcut cannot be configured by the user.

	Array events;

	Key keycode = p_keycode;

#ifdef MACOS_ENABLED
	// Use Cmd+Backspace as a general replacement for Delete shortcuts on macOS
	// (Godot does this too internally but that stuff isn't exposed to us)
	if (keycode == ::godot::KEY_DELETE) {
		keycode = ::godot::KEY_MASK_META | ::godot::KEY_BACKSPACE;
	}
#endif

	Ref<InputEventKey> ie;
	if (keycode != ::godot::KEY_NONE) {
		ie = create_input_event_from_key(keycode, false);
		events.push_back(ie);
	}

	Ref<Shortcut> sc;
	sc.instantiate();
	sc->set_name(p_name);
	sc->set_events(events);
	sc->set_meta("original", events.duplicate(true));
	return sc;
#endif
}

} // namespace zylann::godot
