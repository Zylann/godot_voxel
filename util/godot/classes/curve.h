#ifndef ZN_GODOT_CURVE_H
#define ZN_GODOT_CURVE_H

#if defined(ZN_GODOT)
#include <scene/resources/curve.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/curve.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_CURVE_H
