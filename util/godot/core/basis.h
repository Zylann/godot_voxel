#ifndef ZN_GODOT_BASIS_H
#define ZN_GODOT_BASIS_H

#if defined(ZN_GODOT)
#include <core/math/basis.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/variant/basis.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_BASIS_H
