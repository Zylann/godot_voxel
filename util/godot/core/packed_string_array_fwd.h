#ifndef ZN_GODOT_PACKED_STRING_ARRAY_FWD_H
#define ZN_GODOT_PACKED_STRING_ARRAY_FWD_H

#if defined(ZN_GODOT)
class String;

template <typename T>
class Vector;
typedef Vector<String> PackedStringArray;

#elif defined(ZN_GODOT_EXTENSION)
#include "../macros.h"
ZN_GODOT_FORWARD_DECLARE(PackedStringArray);

#endif

#endif // ZN_GODOT_PACKED_STRING_ARRAY_FWD_H
