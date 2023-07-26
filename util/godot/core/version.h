#ifndef ZN_GODOT_VERSION_H
#define ZN_GODOT_VERSION_H

#if defined(ZN_GODOT)
#include <core/version.h>
#elif defined(ZN_GODOT_EXTENSION)
// TODO GDX: No Godot version macros available! Can't workaround compatibility breakage of Godot's APIs...
#endif

#endif // ZN_GODOT_VERSION_H
