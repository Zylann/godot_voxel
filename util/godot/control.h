#ifndef ZN_GODOT_CONTROL_H
#define ZN_GODOT_CONTROL_H

#if defined(ZN_GODOT)
#include <scene/gui/control.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/control.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_CONTROL_H
