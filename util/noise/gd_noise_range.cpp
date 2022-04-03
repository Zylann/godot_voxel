#include "gd_noise_range.h"

namespace zylann {

math::Interval get_range_2d(const Noise &noise, math::Interval x, math::Interval y) {
	// TODO Implement Godot's Noise range analysis
	return { -1.f, 1.f };
}

math::Interval get_range_3d(const Noise &noise, math::Interval x, math::Interval y, math::Interval z) {
	// TODO Implement Godot's Noise range analysis
	return { -1.f, 1.f };
}

} // namespace zylann
