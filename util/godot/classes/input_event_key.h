#ifndef ZN_GODOT_INPUT_EVENT_KEY_H
#define ZN_GODOT_INPUT_EVENT_KEY_H

#if defined(ZN_GODOT)
#include <core/input/input_event.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/input_event_key.hpp>
using namespace godot;
#endif

namespace zylann {

// TODO GDX: InputEventKey::create_reference is not exposed
Ref<InputEventKey> create_input_event_from_key(Key p_keycode_with_modifier_masks, bool p_physical = false);

} // namespace zylann

#endif // ZN_GODOT_INPUT_EVENT_KEY_H
