#ifndef VOXEL_UTILITY_GODOT_FUNCS_H
#define VOXEL_UTILITY_GODOT_FUNCS_H

#include "../math/vector2f.h"
#include "../math/vector3f.h"
#include "../span.h"

#include <core/variant/variant.h>

namespace zylann {

// Specialized copy functions for vectors because they use `real_t`, which can be either `float` or `double`
void copy_to(Vector<Vector3> &dst, const std::vector<Vector3f> &src);
void copy_to(Vector<Vector2> &dst, const std::vector<Vector2f> &src);

template <typename T>
void raw_copy_to(Vector<T> &to, const std::vector<T> &from) {
	to.resize(from.size());
	// resize can fail in case allocation was not possible
	ERR_FAIL_COND(from.size() != static_cast<size_t>(to.size()));
	memcpy(to.ptrw(), from.data(), from.size() * sizeof(T));
}

inline Vector3 get_forward(const Transform3D &t) {
	return -t.basis.get_column(Vector3::AXIS_Z);
}

inline Vector3 to_godot(const Vector3f v) {
	return Vector3(v.x, v.y, v.z);
}

// template <typename T>
// Span<const T> to_span_const(const Vector<T> &a) {
// 	return Span<const T>(a.ptr(), 0, a.size());
// }

inline Span<const Vector2> to_span(const PackedVector2Array &a) {
	return Span<const Vector2>(a.ptr(), a.size());
}

inline Span<const Vector3> to_span(const PackedVector3Array &a) {
	return Span<const Vector3>(a.ptr(), a.size());
}

inline Span<const int> to_span(const PackedInt32Array &a) {
	return Span<const int>(a.ptr(), a.size());
}

inline String ptr2s(const void *p) {
	return String::num_uint64((uint64_t)p, 16);
}

template <typename T>
inline T try_get(const Dictionary &d, String key) {
	const Variant *v = d.getptr(key);
	if (v == nullptr) {
		return T();
	}
	return *v;
}

} // namespace zylann

#endif // VOXEL_UTILITY_GODOT_FUNCS_H
