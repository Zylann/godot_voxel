#ifndef ZN_GODOT_SCENE_TREE_H
#define ZN_GODOT_SCENE_TREE_H

#if defined(ZN_GODOT)
#include <scene/main/scene_tree.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/scene_tree.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_SCENE_TREE_H
