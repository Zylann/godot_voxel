#ifndef ZN_GODOT_TIME_H
#define ZN_GODOT_TIME_H

#if defined(ZN_GODOT)
#include <core/os/time.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/time.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_TIME_H
