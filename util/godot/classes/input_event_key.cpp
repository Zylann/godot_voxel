#include "input_event_key.h"
#include "../core/keyboard.h"

namespace zylann {

Ref<InputEventKey> create_input_event_from_key(Key p_keycode_with_modifier_masks, bool p_physical) {
#if defined(ZN_GODOT)
	return InputEventKey::create_reference(p_keycode_with_modifier_masks, p_physical);

#elif defined(ZN_GODOT_EXTENSION)
	Key p_keycode = p_keycode_with_modifier_masks;

	// Ported from core `InputEventKey::create_reference`

	Ref<InputEventKey> ie;
	ie.instantiate();
	if (p_physical) {
		ie->set_physical_keycode(p_keycode & godot::KEY_CODE_MASK);
	} else {
		ie->set_keycode(p_keycode & godot::KEY_CODE_MASK);
	}

	ie->set_unicode(char32_t(p_keycode & godot::KEY_CODE_MASK));

	if ((p_keycode & godot::KEY_MASK_SHIFT) != godot::KEY_NONE) {
		ie->set_shift_pressed(true);
	}
	if ((p_keycode & godot::KEY_MASK_ALT) != godot::KEY_NONE) {
		ie->set_alt_pressed(true);
	}
	if ((p_keycode & godot::KEY_MASK_CMD_OR_CTRL) != godot::KEY_NONE) {
		ie->set_command_or_control_autoremap(true);
		if ((p_keycode & godot::KEY_MASK_CTRL) != godot::KEY_NONE ||
				(p_keycode & godot::KEY_MASK_META) != godot::KEY_NONE) {
			WARN_PRINT("Invalid Key Modifiers: Command or Control autoremapping is enabled, Meta and Control values "
					   "are ignored!");
		}
	} else {
		if ((p_keycode & godot::KEY_MASK_CTRL) != godot::KEY_NONE) {
			ie->set_ctrl_pressed(true);
		}
		if ((p_keycode & godot::KEY_MASK_META) != godot::KEY_NONE) {
			ie->set_meta_pressed(true);
		}
	}

	return ie;
#endif
}

} // namespace zylann
