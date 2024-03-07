#ifndef ZN_GODOT_RESOURCE_H
#define ZN_GODOT_RESOURCE_H

#if defined(ZN_GODOT)
#include <core/io/resource.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/resource.hpp>
using namespace godot;
#endif

namespace zylann::godot {

// Godot doesn't have configuration warnings yet on resources.
// But when we add them and they are nested, it can be difficult to put them into context when the warning appears in
// the scene tree. This helper prepends context so we can nest configuration warnings in resources.
// Context is a callable template returning something convertible to a String, so it only gets evaluated when there
// actually are warnings.
template <typename TResource, typename FContext>
inline void get_resource_configuration_warnings(const TResource &resource, PackedStringArray &warnings, FContext f) {
	const int prev_size = warnings.size();

	// This method is by us, not Godot.
	resource.get_configuration_warnings(warnings);

	const int current_size = warnings.size();
	if (current_size != prev_size) {
		String context = f();
		for (int i = prev_size; i < current_size; ++i) {
			String w = context + warnings[i];
#if defined(ZN_GODOT)
			warnings.write[i] = w;
#elif defined(ZN_GODOT_EXTENSION)
			warnings[i] = w;
#endif
		}
	}
}

} // namespace zylann::godot

#endif // ZN_GODOT_RESOURCE_H
