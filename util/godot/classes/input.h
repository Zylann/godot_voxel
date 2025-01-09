#ifndef ZN_GODOT_INPUT_H
#define ZN_GODOT_INPUT_H

#if defined(ZN_GODOT)
#include <core/input/input.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/input.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_INPUT_H
