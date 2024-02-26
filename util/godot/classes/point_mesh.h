#ifndef ZN_GODOT_POINT_MESH_H
#define ZN_GODOT_POINT_MESH_H

#if defined(ZN_GODOT)
#include <core/version.h>

#if VERSION_MAJOR == 4 && VERSION_MINOR <= 2
#include <scene/resources/primitive_meshes.h>
#else
#include <scene/resources/3d/primitive_meshes.h>
#endif

#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/point_mesh.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_POINT_MESH_H
