#include "funcs.h"
#include "../math/conv.h"
#include "../profiling.h"

#include <core/config/engine.h>
#include <scene/main/node.h>
#include <scene/resources/multimesh.h>

namespace zylann {

void copy_to(Vector<Vector3> &dst, const std::vector<Vector3f> &src) {
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

void copy_to(Vector<Vector2> &dst, const std::vector<Vector2f> &src) {
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

} // namespace zylann
