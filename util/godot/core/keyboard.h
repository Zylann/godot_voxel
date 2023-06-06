#ifndef ZN_GODOT_KEYBOARD_H
#define ZN_GODOT_KEYBOARD_H

// Key enums are not defined the same way between Godot and GDExtension.
// This defines aliases so using them is the same in both module and extension builds.

#if defined(ZN_GODOT)
#include <core/os/keyboard.h>

namespace godot {
static const KeyModifierMask KEY_CODE_MASK = KeyModifierMask::CODE_MASK;
static const KeyModifierMask KEY_MODIFIER_MASK = KeyModifierMask::MODIFIER_MASK;
static const KeyModifierMask KEY_MASK_CMD_OR_CTRL = KeyModifierMask::CMD_OR_CTRL;
static const KeyModifierMask KEY_MASK_SHIFT = KeyModifierMask::SHIFT;
static const KeyModifierMask KEY_MASK_ALT = KeyModifierMask::ALT;
static const KeyModifierMask KEY_MASK_META = KeyModifierMask::META;
static const KeyModifierMask KEY_MASK_CTRL = KeyModifierMask::CTRL;
static const KeyModifierMask KEY_MASK_KPAD = KeyModifierMask::KPAD;
static const KeyModifierMask KEY_MASK_GROUP_SWITCH = KeyModifierMask::GROUP_SWITCH;

static const Key KEY_NONE = Key::NONE;
static const Key KEY_R = Key::R;
}; // namespace godot

#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/global_constants.hpp>

// TODO GDX: The operator `Key & KeyModifierMask` is defined in core, but not in GDExtension...
constexpr godot::Key operator&(godot::Key a, godot::KeyModifierMask b) {
	return (godot::Key)((int)a & (int)b);
}

// TODO GDX: The operator `Key | KeyModifierMask` is defined in core, but not in GDExtension...
constexpr godot::Key operator|(godot::KeyModifierMask a, godot::Key b) {
	return (godot::Key)((int)a | (int)b);
}

// TODO GDX: The operator `KeyModifierMask | KeyModifierMask` is defined in core, but not in GDExtension...
constexpr godot::KeyModifierMask operator|(godot::KeyModifierMask a, godot::KeyModifierMask b) {
	return (godot::KeyModifierMask)((int)a | (int)b);
}

#endif

#endif // ZN_GODOT_KEYBOARD_H
