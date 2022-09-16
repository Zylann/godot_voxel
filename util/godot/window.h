#ifndef ZN_GODOT_WINDOW_H
#define ZN_GODOT_WINDOW_H

#if defined(ZN_GODOT)
#include <scene/main/window.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/window.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_WINDOW_H
