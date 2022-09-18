#ifndef ZN_GODOT_DISPLAY_SERVER_H
#define ZN_GODOT_DISPLAY_SERVER_H

#if defined(ZN_GODOT)
#include <servers/display_server.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/display_server.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_DISPLAY_SERVER_H
