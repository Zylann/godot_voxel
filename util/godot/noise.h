#ifndef ZN_GODOT_NOISE_H
#define ZN_GODOT_NOISE_H

#if defined(ZN_GODOT)
#include <modules/noise/noise.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/noise.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_NOISE_H
