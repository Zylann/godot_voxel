#ifndef ZN_CONV_H
#define ZN_CONV_H

#include "vector2f.h"
#include "vector3.h"
#include "vector3d.h"
#include "vector3f.h"
#include "vector3i.h"

namespace zylann {

// Explicit conversion methods. Not in respective files because it would cause circular dependencies.

// Note, in Godot this is an implicit conversion. But I dont like implicit
inline Vector3i to_vec3i(Vector3 v) {
	return Vector3i(v.x, v.y, v.z);
}

inline Vector3i to_vec3i(Vector3f v) {
	return Vector3i(v.x, v.y, v.z);
}

inline Vector3f to_vec3f(Vector3i v) {
	return Vector3f(v.x, v.y, v.z);
}

inline Vector3f to_vec3f(Vector3 v) {
	return Vector3f(v.x, v.y, v.z);
}

inline Vector3 to_vec3(const Vector3f v) {
	return Vector3(v.x, v.y, v.z);
}

inline Vector3d to_vec3d(const Vector3f v) {
	return Vector3d(v.x, v.y, v.z);
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

} // namespace math
} // namespace zylann

#endif // ZN_CONV_H
