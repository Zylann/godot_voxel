#ifndef ZN_GODOT_ENGINE_H
#define ZN_GODOT_ENGINE_H

#if defined(ZN_GODOT)
#include <core/config/engine.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/engine.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_ENGINE_H
