#ifndef ZN_GODOT_OBJECT_H
#define ZN_GODOT_OBJECT_H

#if defined(ZN_GODOT)
#include <core/object/object.h>
// The `GDCLASS` macro isn't compiling when inheriting `Object`, unless `class_db.h` is also included
#include <core/object/class_db.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/object.hpp>
// The `GDCLASS` macro isn't compiling when inheriting `Object`, unless `class_db.hpp` is also included
#include <godot_cpp/core/class_db.hpp>
using namespace godot;
#endif

#include "../../containers/std_vector.h"

#ifdef ZN_GODOT_EXTENSION
// TODO GDX: `MAKE_RESOURCE_TYPE_HINT` is not available in GodotCpp
// Helper macro to use with PROPERTY_HINT_ARRAY_TYPE for arrays of specific resources:
// PropertyInfo(Variant::ARRAY, "fallbacks", PROPERTY_HINT_ARRAY_TYPE, MAKE_RESOURCE_TYPE_HINT("Font")
#define MAKE_RESOURCE_TYPE_HINT(m_type) vformat("%s/%s:%s", Variant::OBJECT, PROPERTY_HINT_RESOURCE_TYPE, m_type)
#endif

namespace zylann::godot {

// Turns out these functions are only used in editor for now.
// They are generic, but I have to wrap them, otherwise GCC throws warnings-as-errors for them being unused.
#ifdef TOOLS_ENABLED

// Gets a hash of a given object from its properties. If properties are objects too, they are recursively
// parsed. Note that restricting to editable properties is important to avoid costly properties with objects
// such as textures or meshes.
uint64_t get_deep_hash(
		const Object &obj,
		uint32_t property_usage = PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_EDITOR,
		uint64_t hash = 0
);

// Getting property info in Godot modules and GDExtension has a different API, with the same information.
struct PropertyInfoWrapper {
	Variant::Type type;
	String name;
	uint32_t usage;
};
void get_property_list(const Object &obj, StdVector<PropertyInfoWrapper> &out_properties);

void set_object_edited(Object &obj);

#endif

} // namespace zylann::godot

#endif // ZN_GODOT_OBJECT_H
