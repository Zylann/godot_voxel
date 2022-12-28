#ifndef ZN_GODOT_PACKED_SCENE_H
#define ZN_GODOT_PACKED_SCENE_H

#if defined(ZN_GODOT)
#include <scene/resources/packed_scene.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/packed_scene.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_PACKED_SCENE_H
