#ifndef ZN_GODOT_GDVIRTUAL_H
#define ZN_GODOT_GDVIRTUAL_H

#if defined(ZN_GODOT)
#include <core/object/script_language.h> // needed for GDVIRTUAL macro
#include <core/object/gdvirtual.gen.inc> // Also needed for GDVIRTUAL macro...
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/core/gdvirtual.gen.inc>
#endif

#endif // ZN_GODOT_GDVIRTUAL_H
