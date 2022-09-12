#ifndef ZN_GODOT_BASIS_H
#define ZN_GODOT_BASIS_H

#if defined(ZN_GODOT)
#include <core/math/basis.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/variant/basis.hpp>
using namespace godot;
#endif

namespace zylann {

inline Quaternion get_rotation_quaternion(const Basis &b) {
#if defined(ZN_GODOT)
	return b.get_rotation_quaternion();
#elif defined(ZN_GODOT_EXTENSION)
	return b.get_rotation_quat();
#endif
}

} //namespace zylann

#endif // ZN_GODOT_BASIS_H
