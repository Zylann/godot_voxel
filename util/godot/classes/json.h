#ifndef ZN_GODOT_JSON_H
#define ZN_GODOT_JSON_H

#if defined(ZN_GODOT)
#include <core/io/json.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/json.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_JSON_H
