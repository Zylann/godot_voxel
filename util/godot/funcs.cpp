#include "funcs.h"
#include "../math/conv.h"
#include "../profiling.h"

namespace zylann {

void copy_to(PackedVector3Array &dst, const std::vector<Vector3f> &src) {
	dst.resize(src.size());
	// resize can fail in case allocation was not possible
	ERR_FAIL_COND(dst.size() != static_cast<int>(src.size()));

#ifdef REAL_T_IS_DOUBLE
	// Convert floats to doubles
	const unsigned int count = dst.size() * Vector3f::AXIS_COUNT;
	real_t *dst_w = reinterpret_cast<real_t *>(dst.ptrw());
	const float *src_r = reinterpret_cast<const float *>(src.data());
	for (unsigned int i = 0; i < count; ++i) {
		dst_w[i] = src_r[i];
	}
#else
	static_assert(sizeof(Vector3) == sizeof(Vector3f));
	memcpy(dst.ptrw(), reinterpret_cast<const Vector3 *>(src.data()), src.size() * sizeof(Vector3f));
#endif
}

void copy_to(PackedVector2Array &dst, const std::vector<Vector2f> &src) {
	dst.resize(src.size());
	// resize can fail in case allocation was not possible
	ERR_FAIL_COND(dst.size() != static_cast<int>(src.size()));

#ifdef REAL_T_IS_DOUBLE
	// Convert floats to doubles
	const unsigned int count = dst.size() * Vector2f::AXIS_COUNT;
	real_t *dst_w = reinterpret_cast<real_t *>(dst.ptrw());
	const float *src_r = reinterpret_cast<const float *>(src.data());
	for (unsigned int i = 0; i < count; ++i) {
		dst_w[i] = src_r[i];
	}
#else
	static_assert(sizeof(Vector2) == sizeof(Vector2f));
	memcpy(dst.ptrw(), reinterpret_cast<const Vector2 *>(src.data()), src.size() * sizeof(Vector2f));
#endif
}

template <typename PackedVector_T, typename T>
inline void copy_to_template(PackedVector_T &dst, Span<const T> src) {
	dst.resize(src.size());
#ifdef DEBUG_ENABLED
	ZN_ASSERT(size_t(dst.size()) == src.size());
#endif
	T *dst_data = dst.ptrw();
	//static_assert(sizeof(dst_data) == sizeof(T));
	memcpy(dst_data, src.data(), src.size() * sizeof(T));
}

void copy_to(PackedVector3Array &dst, const std::vector<Vector3> &src) {
	copy_to_template(dst, to_span(src));
}

void copy_to(PackedVector3Array &dst, Span<const Vector3> src) {
	copy_to_template(dst, src);
}

void copy_to(PackedInt32Array &dst, const std::vector<int32_t> &src) {
	copy_to_template(dst, to_span(src));
}

void copy_to(PackedInt32Array &dst, Span<const int32_t> src) {
	copy_to_template(dst, src);
}

void copy_to(PackedColorArray &dst, const std::vector<Color> &src) {
	copy_to_template(dst, to_span(src));
}

void copy_to(PackedFloat32Array &dst, const std::vector<float> &src) {
	copy_to_template(dst, to_span(src));
}

void copy_to(PackedColorArray &dst, Span<const Color> src) {
	copy_to_template(dst, src);
}

void copy_to(PackedByteArray &dst, Span<const uint8_t> src) {
	copy_to_template(dst, src);
}

void copy_to(Span<uint8_t> dst, const PackedByteArray &src) {
	const size_t src_size = src.size();
	ZN_ASSERT(dst.size() == src_size);
	const uint8_t *src_data = src.ptr();
	ZN_ASSERT(src_data != nullptr);
	memcpy(dst.data(), src_data, src_size);
}

} // namespace zylann
