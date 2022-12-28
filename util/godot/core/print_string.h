#ifndef ZN_GODOT_PRINT_STRING_H
#define ZN_GODOT_PRINT_STRING_H

// Access to `print_line`

#if defined(ZN_GODOT)
#include <core/string/print_string.h>

#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/variant/utility_functions.hpp>

inline void print_line(const Variant &v) {
	UtilityFunctions::print(v);
}

#endif

#endif // ZN_GODOT_PRINT_STRING_H
