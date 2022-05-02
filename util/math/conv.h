#ifndef ZN_CONV_H
#define ZN_CONV_H

#include "vector2f.h"
#include "vector3.h"
#include "vector3f.h"
#include "vector3i.h"

namespace zylann {

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

} // namespace zylann

#endif // ZN_CONV_H
