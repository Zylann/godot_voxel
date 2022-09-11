#ifndef ZYLANN_VECTOR3F_H
#define ZYLANN_VECTOR3F_H

#include "../errors.h"
#include "vector3t.h"
#include <iosfwd>

namespace zylann {

// 32-bit float precision 3D vector.
// Because Godot's `Vector3` uses `real_t`, so when `real_t` is `double` it forces some things to use double-precision
// vectors while they dont need that amount of precision. This is also a problem for some third-party libraries
// that do not support `double` as a result.
typedef Vector3T<float> Vector3f;

namespace math {

inline Vector3f floor(const Vector3f a) {
	return Vector3f(Math::floor(a.x), Math::floor(a.y), Math::floor(a.z));
}

inline Vector3f ceil(const Vector3f a) {
	return Vector3f(Math::ceil(a.x), Math::ceil(a.y), Math::ceil(a.z));
}

inline Vector3f lerp(const Vector3f a, const Vector3f b, const float t) {
	return Vector3f(Math::lerp(a.x, b.x, t), Math::lerp(a.y, b.y, t), Math::lerp(a.z, b.z, t));
}

inline bool has_nan(const Vector3f &v) {
	return is_nan(v.x) || is_nan(v.y) || is_nan(v.z);
}

inline Vector3f normalized(const Vector3f &v) {
	const float lengthsq = length_squared(v);
	if (lengthsq == 0) {
		return Vector3f();
	} else {
		const float length = Math::sqrt(lengthsq);
		return v / length;
	}
}

inline bool is_normalized(const Vector3f &v) {
	// use length_squared() instead of length() to avoid sqrt(), makes it more stringent.
	return Math::is_equal_approx(length_squared(v), 1, float(UNIT_EPSILON));
}

} // namespace math

std::stringstream &operator<<(std::stringstream &ss, const Vector3f &v);

} // namespace zylann

#endif // ZYLANN_VECTOR3F_H
