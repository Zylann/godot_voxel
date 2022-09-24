#ifndef ZN_GODOT_OS_H
#define ZN_GODOT_OS_H

#if defined(ZN_GODOT)
#include <core/os/os.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/os.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_OS_H
