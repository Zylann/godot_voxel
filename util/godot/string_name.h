#ifndef ZN_GODOT_STRING_NAME_H
#define ZN_GODOT_STRING_NAME_H

#if defined(ZN_GODOT)
#include <core/string/string_name.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/variant/string_name.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_STRING_NAME_H
