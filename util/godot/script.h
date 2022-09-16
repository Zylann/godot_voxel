#ifndef ZN_GODOT_SCRIPT_H
#define ZN_GODOT_SCRIPT_H

#if defined(ZN_GODOT)
#include <core/object/script_language.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/script.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_SCRIPT_H
