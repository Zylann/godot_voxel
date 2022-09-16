#ifndef ZN_MATH_TRANSFORM_3D_H
#define ZN_MATH_TRANSFORM_3D_H

#if defined(ZN_GODOT)
#include <core/math/transform_3d.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/variant/transform3d.hpp>
using namespace godot;
#endif

namespace zylann {

// TODO GDX: Transform3D::translated_local() is missing
inline Transform3D transform3d_translated_local(const Transform3D transform, const Vector3 &p_translation) {
#if defined(ZN_GODOT)
	return transform.translated_local(p_translation);
#elif defined(ZN_GODOT_EXTENSION)
	return Transform3D(transform.basis, transform.origin + transform.basis.xform(p_translation));
#endif
}

} //namespace zylann

#endif // ZN_MATH_TRANSFORM_3D_H
