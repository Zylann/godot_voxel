#ifndef ZN_CONV_H
#define ZN_CONV_H

#include "transform3f.h"
#include "transform_3d.h"
#include "vector2.h"
#include "vector2f.h"
#include "vector2i.h"
#include "vector3.h"
#include "vector3d.h"
#include "vector3f.h"
#include "vector3i.h"
#include "vector3i16.h"
#include <limits>

namespace zylann {

// Explicit conversion methods. Not in respective files because it would cause circular dependencies.

// Godot => Godot
// Note, in Godot modules there are implicit conversions. But I dont like implicit.

inline Vector2i to_vec2i(const Vector2 v) {
	return Vector2i(v.x, v.y);
}

inline Vector3i to_vec3i(Vector3 v) {
	return Vector3i(v.x, v.y, v.z);
}

inline Vector3 to_vec3(const Vector3i v) {
	return Vector3(v.x, v.y, v.z);
}

// Godot => ZN

inline Vector2f to_vec2f(Vector2 v) {
	return Vector2f(v.x, v.y);
}

inline Vector2f to_vec2f(Vector2i v) {
	return Vector2f(v.x, v.y);
}

inline Vector3f to_vec3f(Vector3i v) {
	return Vector3f(v.x, v.y, v.z);
}

inline Vector3f to_vec3f(Vector3 v) {
	return Vector3f(v.x, v.y, v.z);
}

inline Vector3i16 to_vec3i16(const Vector3i v) {
	return Vector3i16(v.x, v.y, v.z);
}

inline Basis3f to_basis3f(const Basis &src) {
	Basis3f dst;
	dst.rows[0] = to_vec3f(src.rows[0]);
	dst.rows[1] = to_vec3f(src.rows[1]);
	dst.rows[2] = to_vec3f(src.rows[2]);
	return dst;
}

inline Transform3f to_transform3f(const Transform3D &t) {
	return Transform3f(to_basis3f(t.basis), to_vec3f(t.origin));
}

// ZN => Godot

template <typename T>
inline Vector2i to_vec2i(const Vector2T<T> v) {
	return Vector2i(v.x, v.y);
}

template <typename T>
inline Vector3i to_vec3i(const Vector3T<T> v) {
	return Vector3i(v.x, v.y, v.z);
}

template <typename T>
inline Vector3 to_vec3(const Vector3T<T> v) {
	return Vector3(v.x, v.y, v.z);
}

inline Basis to_basis3(const Basis3f &src) {
	Basis dst;
	dst.rows[0] = to_vec3(src.rows[0]);
	dst.rows[1] = to_vec3(src.rows[1]);
	dst.rows[2] = to_vec3(src.rows[2]);
	return dst;
}

inline Transform3D to_transform3(const Transform3f &t) {
	return Transform3D(to_basis3(t.basis), to_vec3(t.origin));
}

// ZN => ZN

template <typename T>
inline Vector3d to_vec3d(const Vector3T<T> v) {
	return Vector3d(v.x, v.y, v.z);
}

inline bool can_convert_to_i16(Vector3i p) {
	return p.x >= std::numeric_limits<int16_t>::min() && p.x <= std::numeric_limits<int16_t>::max() &&
			p.y >= std::numeric_limits<int16_t>::min() && p.y <= std::numeric_limits<int16_t>::max() &&
			p.z >= std::numeric_limits<int16_t>::min() && p.z <= std::numeric_limits<int16_t>::max();
}

namespace math {

inline Vector3i floor_to_int(const Vector3 &f) {
	return Vector3i(Math::floor(f.x), Math::floor(f.y), Math::floor(f.z));
}

inline Vector3i floor_to_int(const Vector3f &f) {
	return Vector3i(Math::floor(f.x), Math::floor(f.y), Math::floor(f.z));
}

inline Vector3i round_to_int(const Vector3 &f) {
	return Vector3i(Math::round(f.x), Math::round(f.y), Math::round(f.z));
}

inline Vector3i ceil_to_int(const Vector3 &f) {
	return Vector3i(Math::ceil(f.x), Math::ceil(f.y), Math::ceil(f.z));
}

inline Vector3i ceil_to_int(const Vector3f &f) {
	return Vector3i(Math::ceil(f.x), Math::ceil(f.y), Math::ceil(f.z));
}

} // namespace math
} // namespace zylann

#endif // ZN_CONV_H
