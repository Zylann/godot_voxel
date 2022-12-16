#ifndef ZYLANN_VECTOR4T_H
#define ZYLANN_VECTOR4T_H

#include "../errors.h"
#include "funcs.h"

namespace zylann {

// Template 4-dimensional vector. Only fields and standard operators.
// Math functions are separate to allow more unified overloading, and similarity with other math libraries such as
// shaders.
template <typename T>
struct Vector4T {
	enum Axis { //
		AXIS_X = 0,
		AXIS_Y = 1,
		AXIS_Z = 2,
		AXIS_W = 3,
		AXIS_COUNT = 4
	};

	union {
		struct {
			T x;
			T y;
			T z;
			T w;
		};
		T coords[4];
	};

	Vector4T() : x(0), y(0), z(0), w(0) {}

	// It is recommended to use `explicit` because otherwise it would open the door to plenty of implicit conversions
	// which would make many cases ambiguous.
	explicit Vector4T(T p_v) : x(p_v), y(p_v), z(p_v), w(p_v) {}

	Vector4T(T p_x, T p_y, T p_z, T p_w) : x(p_x), y(p_y), z(p_z), w(p_w) {}

	inline const T &operator[](const unsigned int p_axis) const {
#ifdef DEBUG_ENABLED
		ZN_ASSERT(p_axis < AXIS_COUNT);
#endif
		return coords[p_axis];
	}

	inline T &operator[](const unsigned int p_axis) {
#ifdef DEBUG_ENABLED
		ZN_ASSERT(p_axis < AXIS_COUNT);
#endif
		return coords[p_axis];
	}
};

} // namespace zylann

#endif // ZYLANN_VECTOR4T_H
