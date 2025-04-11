#ifndef ZN_GODOT_VECTOR3_H
#define ZN_GODOT_VECTOR3_H

#if defined(ZN_GODOT)
#include <core/math/vector3.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/variant/vector3.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_VECTOR3_H
