#ifndef ZN_GODOT_AABB_H
#define ZN_GODOT_AABB_H

#if defined(ZN_GODOT)
#include <core/math/aabb.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/variant/aabb.hpp>
using namespace godot;
#endif

#include "../../string/std_stringstream.h"

namespace zylann {

StdStringStream &operator<<(StdStringStream &ss, const AABB &v);

inline real_t distance_squared(const AABB &aabb, const Vector3 p) {
	const Vector3 d = (aabb.position - p).max(p - (aabb.position + aabb.size)).max(Vector3());
	return d.length_squared();
}

} // namespace zylann

#endif // ZN_GODOT_AABB_H
