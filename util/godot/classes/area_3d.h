#ifndef ZN_GODOT_AREA_3D_H
#define ZN_GODOT_AREA_3D_H

#if defined(ZN_GODOT)
#include <core/version.h>

#if VERSION_MAJOR == 4 && VERSION_MINOR <= 2
#include <scene/3d/area_3d.h>
#else
#include <scene/3d/physics/area_3d.h>
#endif

#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/accept_dialog.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_AREA_3D_H
