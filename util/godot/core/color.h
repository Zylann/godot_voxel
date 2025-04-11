#ifndef ZN_COLOR_H
#define ZN_COLOR_H

#if defined(ZN_GODOT)
#include <core/math/color.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/variant/color.hpp>
using namespace godot;
#endif

#endif // ZN_COLOR_H
