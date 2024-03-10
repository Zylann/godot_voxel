#ifndef ZN_MATH_VECTOR2I_H
#define ZN_MATH_VECTOR2I_H

#if defined(ZN_GODOT)
#include <core/math/vector2i.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/variant/vector2i.hpp>
using namespace godot;
#endif

#include "../errors.h"
#include "../godot/macros.h"
#include "../hash_funcs.h"
#include "../std_stringstream.h"
#include "funcs.h"
#include <functional> // For std::hash

ZN_GODOT_NAMESPACE_BEGIN

inline Vector2i operator&(const Vector2i &a, int b) {
	return Vector2i(a.x & b, a.y & b);
}

ZN_GODOT_NAMESPACE_END

namespace zylann {

namespace Vector2iUtil {

inline Vector2i create(int xy) {
	return Vector2i(xy, xy);
}

inline int64_t get_area(const Vector2i v) {
#ifdef DEBUG_ENABLED
	ZN_ASSERT_RETURN_V(v.x >= 0 && v.y >= 0, 0);
#endif
	return v.x * v.y;
}

inline unsigned int get_yx_index(const Vector2i v, const Vector2i area_size) {
	return v.x + v.y * area_size.x;
}

} // namespace Vector2iUtil

namespace math {

inline Vector2i floordiv(const Vector2i v, const Vector2i d) {
	return Vector2i(math::floordiv(v.x, d.x), math::floordiv(v.y, d.y));
}

inline Vector2i floordiv(const Vector2i v, const int d) {
	return Vector2i(math::floordiv(v.x, d), math::floordiv(v.y, d));
}

inline Vector2i ceildiv(const Vector2i v, const int d) {
	return Vector2i(math::ceildiv(v.x, d), math::ceildiv(v.y, d));
}

inline Vector2i ceildiv(const Vector2i v, const Vector2i d) {
	return Vector2i(math::ceildiv(v.x, d.x), math::ceildiv(v.y, d.y));
}

inline int chebyshev_distance(const Vector2i &a, const Vector2i &b) {
	// In Chebyshev metric, points on the sides of a square are all equidistant to its center
	return math::max(Math::abs(a.x - b.x), Math::abs(a.y - b.y));
}

} // namespace math

StdStringStream &operator<<(StdStringStream &ss, const Vector2i &v);

} // namespace zylann

// For STL
namespace std {
template <>
struct hash<Vector2i> {
	size_t operator()(const Vector2i &v) const {
		// TODO This is 32-bit, would it be better if it was 64?
		uint32_t h = zylann::hash_murmur3_one_32(v.x);
		h = zylann::hash_murmur3_one_32(v.y, h);
		return zylann::hash_fmix32(h);
	}
};
} // namespace std

#endif // ZN_MATH_VECTOR2I_H
