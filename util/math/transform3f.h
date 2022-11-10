#ifndef ZN_TRANSFORM3F_H
#define ZN_TRANSFORM3F_H

#include "basis3f.h"

namespace zylann {

struct Transform3f {
	Basis3f basis;
	Vector3f origin;

    inline Transform3f(){}

	inline Transform3f(const Basis3f &p_basis, const Vector3f &p_origin) : basis(p_basis), origin(p_origin) {}

	inline Vector3f xform(const Vector3f v) const {
		return basis.xform(v) + origin;
	}
};

} // namespace zylann

#endif // ZN_TRANSFORM3F_H
