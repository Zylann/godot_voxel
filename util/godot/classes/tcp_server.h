#ifndef ZN_GODOT_TCP_SERVER_H
#define ZN_GODOT_TCP_SERVER_H

#if defined(ZN_GODOT)
#include <core/io/tcp_server.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/tcp_server.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_TCP_SERVER_H
