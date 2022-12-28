#ifndef ZN_GODOT_RESOURCE_H
#define ZN_GODOT_RESOURCE_H

#if defined(ZN_GODOT)
#include <core/io/resource.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/resource.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_RESOURCE_H
