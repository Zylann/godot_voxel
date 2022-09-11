#ifndef ZN_COLOR_H
#define ZN_COLOR_H

#if defined(ZN_GODOT)
#include <core/math/color.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/variant/color.hpp>
using namespace godot;
#endif

namespace zylann::math {

inline Color lerp(const Color a, const Color b, float t) {
	return Color(Math::lerp(a.r, b.r, t), Math::lerp(a.g, b.g, t), Math::lerp(a.b, b.b, t), Math::lerp(a.a, b.a, t));
}

} // namespace zylann::math

#endif // ZN_COLOR_H
