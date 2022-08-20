#ifndef ZN_GODOT_OBJECT_H
#define ZN_GODOT_OBJECT_H

#include <core/object/object.h>

namespace zylann {

// Turns out these functions are only used in editor for now.
// They are generic, but I have to wrap them, otherwise GCC throws warnings-as-errors for them being unused.
#ifdef TOOLS_ENABLED

// Gets a hash of a given object from its properties. If properties are objects too, they are recursively parsed.
// Note that restricting to editable properties is important to avoid costly properties with objects such as textures or
// meshes.
uint64_t get_deep_hash(
		const Object &obj, uint32_t property_usage = PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_EDITOR, uint64_t hash = 0);

#endif

} // namespace zylann

#endif // ZN_GODOT_OBJECT_H
