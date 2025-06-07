#ifndef ZN_GODOT_MATERIAL_H
#define ZN_GODOT_MATERIAL_H

#if defined(ZN_GODOT)
#include <scene/resources/material.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/material.hpp>
using namespace godot;
#endif

namespace zylann::godot {

// Exposing exclusively 3D material types requires a special hint string, because the base class is Material.
extern const char *MATERIAL_3D_PROPERTY_HINT_STRING;

} // namespace zylann::godot

#endif // ZN_GODOT_MATERIAL_H
