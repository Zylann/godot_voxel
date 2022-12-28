#ifndef ZN_GODOT_TIMER_H
#define ZN_GODOT_TIMER_H

#if defined(ZN_GODOT)
#include <scene/main/timer.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/timer.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_TIMER_H
