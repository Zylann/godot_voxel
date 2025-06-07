#ifndef ZN_GODOT_BASIS_H
#define ZN_GODOT_BASIS_H

#if defined(ZN_GODOT)
#include <core/math/basis.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/variant/basis.hpp>
using namespace godot;
#endif

namespace zylann::godot::BasisUtility {

inline Vector3 get_forward(const Basis &basis) {
	return -basis.get_column(Vector3::AXIS_Z);
}

inline Vector3 get_up(const Basis &basis) {
	return basis.get_column(Vector3::AXIS_Y);
}

} // namespace zylann::godot::BasisUtility

#endif // ZN_GODOT_BASIS_H
