#ifndef ZN_GODOT_STRING_NAME_H
#define ZN_GODOT_STRING_NAME_H

#if defined(ZN_GODOT)
#include <core/string/string_name.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/variant/string_name.hpp>
using namespace godot;
#endif

// Also gave up trying to make an `operator<<(stringstream, StringName)` overload, the billion conversions it has (and
// does not have in GDExtension) makes it impossible to compile without ambiguity...

#endif // ZN_GODOT_STRING_NAME_H
