#ifndef ZN_GODOT_H_BOX_CONTAINER_H
#define ZN_GODOT_H_BOX_CONTAINER_H

#if defined(ZN_GODOT)
#include <scene/gui/box_container.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/h_box_container.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_H_BOX_CONTAINER_H
