#ifndef ZN_GODOT_DICTIONARY_H
#define ZN_GODOT_DICTIONARY_H

#if defined(ZN_GODOT)
#include <core/variant/dictionary.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/variant/dictionary.hpp>
using namespace godot;
#endif

namespace zylann::godot {

template <typename T>
inline bool try_get(const Dictionary &d, const Variant &key, T &out_value) {
#if defined(ZN_GODOT)
	const Variant *v = d.getptr(key);
	if (v == nullptr) {
		return false;
	}
	// TODO There is no easy way to return `false` if the value doesn't have the right type...
	// Because multiple C++ types match Variant types, and Variant types match multiple C++ types, and silently convert
	// between them.
	out_value = *v;
	return true;
#elif defined(ZN_GODOT_EXTENSION)
	Variant v = d.get(key, Variant());
	// TODO GDX: there is no way, in a single lookup, to differenciate an inexistent key and an existing key with the
	// value `null`. So we have to do a second lookup to check what NIL meant.
	if (v.get_type() == Variant::NIL) {
		out_value = T();
		return d.has(key);
	}
	out_value = v;
	return true;
#endif
}

} // namespace zylann::godot

#endif // ZN_GODOT_DICTIONARY_H
