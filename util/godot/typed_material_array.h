#ifndef ZN_GODOT_TYPED_MATERIAL_ARRAY_H
#define ZN_GODOT_TYPED_MATERIAL_ARRAY_H

// TODO GDX: TypedArray<T> is missing from GodotCpp, so there is no way to bind Arrays with a type hint.
// Defining typedefs to keep code untouched.

#include "material.h"

#if defined(ZN_GODOT)
#include <core/variant/typed_array.h>
typedef TypedArray<Material> GodotMaterialArray;

#elif defined(ZN_GODOT_EXTENSION)
#include "array.h"
typedef Array GodotMaterialArray;
#endif

#endif // ZN_GODOT_TYPED_MATERIAL_ARRAY_H
