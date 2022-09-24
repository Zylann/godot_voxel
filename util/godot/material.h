#ifndef ZN_GODOT_MATERIAL_H
#define ZN_GODOT_MATERIAL_H

#if defined(ZN_GODOT)
#include <scene/resources/material.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/material.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_MATERIAL_H
