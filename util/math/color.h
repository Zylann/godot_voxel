#ifndef ZN_MATH_COLOR_H
#define ZN_MATH_COLOR_H

#include "../godot/core/color.h"

namespace zylann::math {

inline Color lerp(const Color a, const Color b, float t) {
	return Color(Math::lerp(a.r, b.r, t), Math::lerp(a.g, b.g, t), Math::lerp(a.b, b.b, t), Math::lerp(a.a, b.a, t));
}

} // namespace zylann::math

#endif // ZN_MATH_COLOR_H
