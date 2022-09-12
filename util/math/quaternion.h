#ifndef ZN_GODOT_QUATERNION_H
#define ZN_GODOT_QUATERNION_H

#if defined(ZN_GODOT)
#include <core/math/quaternion.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/variant/quaternion.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_QUATERNION_H
