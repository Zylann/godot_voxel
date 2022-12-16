#ifndef ZYLANN_VECTOR3I_H
#define ZYLANN_VECTOR3I_H

#include "../hash_funcs.h"
#include "funcs.h"
#include <functional> // For std::hash

#if defined(ZN_GODOT)
#include <core/math/vector3.h>
#include <core/math/vector3i.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/vector3i.hpp>
using namespace godot;
#endif

#include <iosfwd>

namespace zylann {
namespace Vector3iUtil {

constexpr int AXIS_COUNT = 3;

inline Vector3i create(int xyz) {
	return Vector3i(xyz, xyz, xyz);
}

inline void sort_min_max(Vector3i &a, Vector3i &b) {
	math::sort(a.x, b.x);
	math::sort(a.y, b.y);
	math::sort(a.z, b.z);
}

// Returning a 64-bit integer because volumes can quickly overflow INT_MAX (like 1300^3),
// even though dense volumes of that size will rarely be encountered in this module
inline int64_t get_volume(const Vector3i &v) {
#ifdef DEBUG_ENABLED
	ZN_ASSERT_RETURN_V(v.x >= 0 && v.y >= 0 && v.z >= 0, 0);
#endif
	return v.x * v.y * v.z;
}

inline unsigned int get_zxy_index(const Vector3i &v, const Vector3i area_size) {
	return v.y + area_size.y * (v.x + area_size.x * v.z); // ZXY
}

inline unsigned int get_zxy_index(int x, int y, int z, int sx, int sy) {
	return y + sy * (x + sx * z); // ZXY
}

inline unsigned int get_zyx_index(const Vector3i &v, const Vector3i area_size) {
	return v.x + area_size.x * (v.y + area_size.y * v.z);
}

inline Vector3i from_zxy_index(unsigned int i, const Vector3i area_size) {
	Vector3i pos;
	pos.y = i % area_size.y;
	pos.x = (i / area_size.y) % area_size.x;
	pos.z = i / (area_size.y * area_size.x);
	return pos;
}

inline bool all_members_equal(const Vector3i v) {
	return v.x == v.y && v.y == v.z;
}

inline bool is_unit_vector(const Vector3i v) {
	return Math::abs(v.x) + Math::abs(v.y) + Math::abs(v.z) == 1;
}

inline bool is_valid_size(const Vector3i &s) {
	return s.x >= 0 && s.y >= 0 && s.z >= 0;
}

inline bool is_empty_size(const Vector3i &s) {
	return s.x == 0 || s.y == 0 || s.z == 0;
}

} // namespace Vector3iUtil

namespace math {

inline Vector3i floordiv(const Vector3i v, const Vector3i d) {
	return Vector3i(math::floordiv(v.x, d.x), math::floordiv(v.y, d.y), math::floordiv(v.z, d.z));
}

inline Vector3i floordiv(const Vector3i v, const int d) {
	return Vector3i(math::floordiv(v.x, d), math::floordiv(v.y, d), math::floordiv(v.z, d));
}

inline Vector3i ceildiv(const Vector3i v, const int d) {
	return Vector3i(math::ceildiv(v.x, d), math::ceildiv(v.y, d), math::ceildiv(v.z, d));
}

inline Vector3i ceildiv(const Vector3i v, const Vector3i d) {
	return Vector3i(math::ceildiv(v.x, d.x), math::ceildiv(v.y, d.y), math::ceildiv(v.z, d.z));
}

inline Vector3i wrap(const Vector3i v, const Vector3i d) {
	return Vector3i(math::wrap(v.x, d.x), math::wrap(v.y, d.y), math::wrap(v.z, d.z));
}

inline Vector3i clamp(const Vector3i a, const Vector3i minv, const Vector3i maxv) {
	return Vector3i(
			math::clamp(a.x, minv.x, maxv.x), math::clamp(a.y, minv.y, maxv.y), math::clamp(a.z, minv.z, maxv.z));
}

inline Vector3i abs(const Vector3i v) {
	return Vector3i(Math::abs(v.x), Math::abs(v.y), Math::abs(v.z));
}

inline Vector3i min(const Vector3i a, const Vector3i b) {
	return Vector3i(min(a.x, b.x), min(a.y, b.y), min(a.z, b.z));
}

inline Vector3i max(const Vector3i a, const Vector3i b) {
	return Vector3i(max(a.x, b.x), max(a.y, b.y), max(a.z, b.z));
}

} // namespace math

std::stringstream &operator<<(std::stringstream &ss, const Vector3i &v);

} // namespace zylann

ZN_GODOT_NAMESPACE_BEGIN
// To prevent unintuitive overload-resolution compiler errors, operators overloads should be
// defined in the same namespace as the type they are dealing with... in which case, Godot's namespace.
// The compiler only looks for overrides in the namespace of the arguments (Koenig lookup, is it?).
// https://stackoverflow.com/questions/5195512/namespaces-and-operator-resolution

inline Vector3i operator<<(const Vector3i &a, int b) {
#ifdef DEBUG_ENABLED
	ZN_ASSERT(b >= 0);
#endif
	return Vector3i(a.x << b, a.y << b, a.z << b);
}

inline Vector3i operator>>(const Vector3i &a, int b) {
#ifdef DEBUG_ENABLED
	ZN_ASSERT(b >= 0);
#endif
	return Vector3i(a.x >> b, a.y >> b, a.z >> b);
}

inline Vector3i operator&(const Vector3i &a, uint32_t b) {
	return Vector3i(a.x & b, a.y & b, a.z & b);
}

ZN_GODOT_NAMESPACE_END

// For Godot
struct Vector3iHasher {
	static inline uint32_t hash(const Vector3i &v) {
		uint32_t hash = zylann::hash_djb2_one_32(v.x);
		hash = zylann::hash_djb2_one_32(v.y, hash);
		return zylann::hash_djb2_one_32(v.z, hash);
	}
};

// For STL
namespace std {
template <>
struct hash<Vector3i> {
	size_t operator()(const Vector3i &v) const {
		return Vector3iHasher::hash(v);
	}
};
} // namespace std

#endif // ZYLANN_VECTOR3I_H
