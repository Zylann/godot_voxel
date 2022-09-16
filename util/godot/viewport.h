#ifndef ZN_GODOT_VIEWPORT_H
#define ZN_GODOT_VIEWPORT_H

#if defined(ZN_GODOT)
#include <scene/main/viewport.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/viewport.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_VIEWPORT_H
