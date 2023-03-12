#ifndef ZN_MATH_VECTOR2I_H
#define ZN_MATH_VECTOR2I_H

#if defined(ZN_GODOT)
#include <core/math/vector2i.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/variant/vector2i.hpp>
using namespace godot;
#endif

#include "../errors.h"

ZN_GODOT_NAMESPACE_BEGIN

inline Vector2i operator&(const Vector2i &a, int b) {
	return Vector2i(a.x & b, a.y & b);
}

ZN_GODOT_NAMESPACE_END

namespace zylann::Vector2iUtil {

inline int64_t get_area(const Vector2i v) {
#ifdef DEBUG_ENABLED
	ZN_ASSERT_RETURN_V(v.x >= 0 && v.y >= 0, 0);
#endif
	return v.x * v.y;
}

} // namespace zylann::Vector2iUtil

#endif // ZN_MATH_VECTOR2I_H
