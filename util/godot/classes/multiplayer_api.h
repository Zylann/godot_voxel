#ifndef ZN_GODOT_MULTIPLAYER_API_H
#define ZN_GODOT_MULTIPLAYER_API_H

#if defined(ZN_GODOT)
#include <scene/main/multiplayer_api.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/multiplayer_api.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_MULTIPLAYER_API_H
