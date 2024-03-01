#ifndef ZN_GODOT_SPHERE_SHAPE_3D_H
#define ZN_GODOT_SPHERE_SHAPE_3D_H

#if defined(ZN_GODOT)
#include <core/version.h>

#if VERSION_MAJOR == 4 && VERSION_MINOR <= 2
#include <scene/resources/sphere_shape_3d.h>
#else
#include <scene/resources/3d/sphere_shape_3d.h>
#endif

#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/sphere_shape_3d.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_SPHERE_SHAPE_3D_H
