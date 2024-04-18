#ifndef ZN_GODOT_NAVIGATION_MESH_SOURCE_GEOMETRY_DATA_3D_H
#define ZN_GODOT_NAVIGATION_MESH_SOURCE_GEOMETRY_DATA_3D_H

#if defined(ZN_GODOT)

#if VERSION_MAJOR == 4 && VERSION_MINOR <= 2
#include <scene/resources/navigation_mesh_source_geometry_data_3d.h>
#else
#include <scene/resources/3d/navigation_mesh_source_geometry_data_3d.h>
#endif

#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/navigation_mesh_source_geometry_data3d.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_NAVIGATION_MESH_SOURCE_GEOMETRY_DATA_3D_H
