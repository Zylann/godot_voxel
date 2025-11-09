#ifndef ZN_GODOT_DISPLAY_SERVER_H
#define ZN_GODOT_DISPLAY_SERVER_H

#if defined(ZN_GODOT)
#include "../core/version.h"

#if GODOT_VERSION_MAJOR == 4 && GODOT_VERSION_MINOR <= 5
#include <servers/display_server.h>
#else
#include <servers/display/display_server.h>
#endif

#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/display_server.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_DISPLAY_SERVER_H
