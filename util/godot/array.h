#ifndef ZN_GODOT_ARRAY_H
#define ZN_GODOT_ARRAY_H

#if defined(ZN_GODOT)
#include <core/variant/array.h>

#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/variant/array.hpp>
using namespace godot;

namespace zylann {

// TODO GDX: GodotCpp does not define `varray` (which is available in modules)

inline godot::Array varray(const godot::Variant &a0) {
	godot::Array a;
	a.append(a0);
	return a;
}

inline godot::Array varray(const godot::Variant &a0, const godot::Variant &a1) {
	godot::Array a;
	a.append(a0);
	a.append(a1);
	return a;
}

inline godot::Array varray(const godot::Variant &a0, const godot::Variant &a1, const godot::Variant &a2) {
	godot::Array a;
	a.append(a0);
	a.append(a1);
	a.append(a2);
	return a;
}

inline godot::Array varray(
		const godot::Variant &a0, const godot::Variant &a1, const godot::Variant &a2, const godot::Variant &a3) {
	godot::Array a;
	a.append(a0);
	a.append(a1);
	a.append(a2);
	a.append(a3);
	return a;
}

} // namespace zylann

#endif // ZN_GODOT_EXTENSION

#endif // ZN_GODOT_ARRAY_H
