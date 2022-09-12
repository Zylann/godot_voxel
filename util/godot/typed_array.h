#ifndef ZN_GODOT_TYPED_ARRAY_H
#define ZN_GODOT_TYPED_ARRAY_H

// TODO GDX: TypedArray<T> is missing from GodotCpp, so there is no way to bind Arrays with a type hint.
// Defining typedefs to keep code untouched.

#if defined(ZN_GODOT)

typedef TypedArray<Material> GodotMaterialArray;

#elif defined(ZN_GODOT_EXTENSION)

#include "array.h"

typedef Array GodotMaterialArray;

#endif

#endif // ZN_GODOT_TYPED_ARRAY_H
