#ifndef ZN_GODOT_CURVE_H
#define ZN_GODOT_CURVE_H

#if defined(ZN_GODOT)
#include <scene/resources/curve.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/curve.hpp>
using namespace godot;
#endif

#include "../../math/interval.h"
#include "../core/version.h"

namespace zylann::godot {

inline math::Interval get_curve_domain(const Curve &curve) {
#if GODOT_VERSION_MAJOR == 4 && GODOT_VERSION_MINOR <= 3
	return math::Interval(0, 1);
#else
	return math::Interval(curve.get_min_domain(), curve.get_max_domain());
#endif
}

} // namespace zylann::godot

#endif // ZN_GODOT_CURVE_H
