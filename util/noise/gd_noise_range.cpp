#include "gd_noise_range.h"

namespace zylann {

// It is not possible to do the same level of range analysis with Godot's `FastNoiseLite`, because it does not expose
// the internal instance of FastNoiseLite, and the official library does not expose some internals either.
// Maybe we could create a temporary instance of our own version and piggyback on it?

math::Interval get_range_2d(const Noise &noise, math::Interval x, math::Interval y) {
	// TODO Implement Godot's Noise range analysis
	return { -1.f, 1.f };
}

math::Interval get_range_3d(const Noise &noise, math::Interval x, math::Interval y, math::Interval z) {
	// TODO Implement Godot's Noise range analysis
	return { -1.f, 1.f };
}

} // namespace zylann
