#ifndef ZN_GODOT_SHORTCUT_H
#define ZN_GODOT_SHORTCUT_H

#if defined(ZN_GODOT)
#include <core/input/shortcut.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/shortcut.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_SHORTCUT_H
