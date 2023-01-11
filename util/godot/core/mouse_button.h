#ifndef ZN_GODOT_MOUSE_BUTTON_H
#define ZN_GODOT_MOUSE_BUTTON_H

#if defined(ZN_GODOT)
#include <core/input/input_enums.h>

#define ZN_GODOT_MouseButton_NONE MouseButton::NONE
#define ZN_GODOT_MouseButton_WHEEL_UP MouseButton::WHEEL_UP
#define ZN_GODOT_MouseButton_WHEEL_DOWN MouseButton::WHEEL_DOWN
#define ZN_GODOT_MouseButtonMask_MIDDLE MouseButtonMask::MIDDLE

#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/global_constants.hpp>
using namespace godot;

#define ZN_GODOT_MouseButton_NONE MouseButton::MOUSE_BUTTON_NONE
#define ZN_GODOT_MouseButton_WHEEL_UP MouseButton::MOUSE_BUTTON_WHEEL_UP
#define ZN_GODOT_MouseButton_WHEEL_DOWN MouseButton::MOUSE_BUTTON_WHEEL_DOWN
#define ZN_GODOT_MouseButtonMask_MIDDLE MouseButtonMask::MOUSE_BUTTON_MASK_MIDDLE

#endif

#endif // ZN_GODOT_MOUSE_BUTTON_H
