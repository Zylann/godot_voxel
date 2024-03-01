#ifndef ZN_GODOT_SHAPE_3D_H
#define ZN_GODOT_SHAPE_3D_H

#if defined(ZN_GODOT)
#include <core/version.h>

#if VERSION_MAJOR == 4 && VERSION_MINOR <= 2
#include <scene/resources/shape_3d.h>
#else
#include <scene/resources/3d/shape_3d.h>
#endif

#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/shape3d.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_SHAPE_3D_H
