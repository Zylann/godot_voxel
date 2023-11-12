#ifndef ZN_MATH_TRANSFORM_3D_H
#define ZN_MATH_TRANSFORM_3D_H

#if defined(ZN_GODOT)
#include <core/math/transform_3d.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/variant/transform3d.hpp>
using namespace godot;
#endif

namespace zylann {

inline Vector3 get_forward(const Transform3D &t) {
	return -t.basis.get_column(Vector3::AXIS_Z);
}

} // namespace zylann

#endif // ZN_MATH_TRANSFORM_3D_H
