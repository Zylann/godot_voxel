#ifndef ZN_GODOT_INPUT_ENUMS_H
#define ZN_GODOT_INPUT_ENUMS_H

#if defined(ZN_GODOT)
#include <core/input/input_enums.h>
namespace godot {
static const MouseButton MOUSE_BUTTON_RIGHT = ::MouseButton::RIGHT;
}
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/global_constants.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_INPUT_ENUMS_H
