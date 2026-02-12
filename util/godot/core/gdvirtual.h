#ifndef ZN_GODOT_GDVIRTUAL_H
#define ZN_GODOT_GDVIRTUAL_H

#if defined(ZN_GODOT)
#include "../core/version.h"

#include <core/object/script_language.h>

#if GODOT_VERSION_MAJOR == 4 && GODOT_VERSION_MINOR <= 6
#include <core/object/gdvirtual.gen.inc>
#else
#include <core/object/gdvirtual.gen.h>
#endif

#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/core/gdvirtual.gen.inc>
#endif

#endif // ZN_GODOT_GDVIRTUAL_H
