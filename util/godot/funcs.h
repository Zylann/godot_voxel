#ifndef VOXEL_UTILITY_GODOT_FUNCS_H
#define VOXEL_UTILITY_GODOT_FUNCS_H

#include "../math/vector2f.h"
#include "../math/vector3f.h"
#include "../span.h"
#include <cstdint>
#include <vector>

#if defined(ZN_GODOT)
#include <core/variant/variant.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/variant/variant.hpp>
using namespace godot;
#endif

namespace zylann {

// Specialized copy functions for vectors because they use `real_t`, which can be either `float` or `double`
void copy_to(PackedVector3Array &dst, const std::vector<Vector3f> &src);
void copy_to(PackedVector2Array &dst, const std::vector<Vector2f> &src);

// Copy functions for matching types.
// Can't have code using template Vector if we want to support compiling both as module and extension.
// So the following are defined for every case instead of a template.
void copy_to(PackedVector3Array &dst, const std::vector<Vector3> &src);
void copy_to(PackedVector3Array &dst, Span<const Vector3> src);
void copy_to(PackedInt32Array &dst, const std::vector<int32_t> &src);
void copy_to(PackedInt32Array &dst, Span<const int32_t> src);
void copy_to(PackedColorArray &dst, const std::vector<Color> &src);
void copy_to(PackedColorArray &dst, Span<const Color> src);
void copy_to(PackedFloat32Array &dst, const std::vector<float> &src);
void copy_to(PackedByteArray &dst, Span<const uint8_t> src);
void copy_to(Span<uint8_t> dst, const PackedByteArray &src);

template <typename T>
inline void copy_bytes_to(PackedByteArray &dst, Span<const T> src) {
	const size_t bytes_count = src.size() * sizeof(T);
	dst.resize(bytes_count);
	uint8_t *dst_w = dst.ptrw();
	ZN_ASSERT(dst_w != nullptr);
	memcpy(dst_w, src.data(), bytes_count);
}

template <typename T>
inline void copy_bytes_to(PackedByteArray &dst, T src) {
	dst.resize(sizeof(T));
	uint8_t *dst_w = dst.ptrw();
	ZN_ASSERT(dst_w != nullptr);
	memcpy(dst_w, &src, sizeof(T));
}

// TODO Can't have code using template Vector if we want to support compiling both as module and extension
#ifdef ZN_GODOT
template <typename T>
void raw_copy_to(Vector<T> &to, const std::vector<T> &from) {
	to.resize(from.size());
	// resize can fail in case allocation was not possible
	ERR_FAIL_COND(from.size() != static_cast<size_t>(to.size()));
	memcpy(to.ptrw(), from.data(), from.size() * sizeof(T));
}
#endif

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

inline Span<const int32_t> to_span(const PackedInt32Array &a) {
	return Span<const int32_t>(a.ptr(), a.size());
}

inline String ptr2s(const void *p) {
	return String::num_uint64((uint64_t)p, 16);
}

template <typename T>
inline bool try_get(const Dictionary &d, String key, T &out_value) {
#if defined(ZN_GODOT)
	const Variant *v = d.getptr(key);
	if (v == nullptr) {
		return false;
	}
	// TODO There is no easy way to return `false` if the value doesn't have the right type...
	// Because multiple C++ types match Variant types, and Variant types match multiple C++ types, and silently convert
	// between them.
	return *v;
#elif defined(ZN_GODOT_EXTENSION)
	Variant v = d.get(key, Variant());
	// TODO GDX: there is no way, in a single lookup, to differenciate an inexistent key and an existing key with the
	// value `null`. So we have to do a second lookup to check what NIL meant.
	if (v.get_type() == Variant::NIL) {
		out_value = T();
		return d.has(key);
	}
	return v;
#endif
}

} // namespace zylann

#endif // VOXEL_UTILITY_GODOT_FUNCS_H
