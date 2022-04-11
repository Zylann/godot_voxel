#ifndef ZN_NOISE_RANGE_UTILITY_H
#define ZN_NOISE_RANGE_UTILITY_H

#include "../math/interval.h"

// Common utilities to obtain interval estimation for noise.
// The main technique is to sample a middle point and use derivatives to find maximum variation.

// TODO We could skew max derivative estimation if the anchor is on a bump or a dip
// because in these cases, it becomes impossible for noise to go further up or further down

namespace zylann {

template <typename Noise_F>
inline math::Interval get_noise_range_2d(
		Noise_F noise_func, const math::Interval &x, const math::Interval &y, float max_derivative) {
	// Any unit vector away from a given evaluation point, the maximum difference is a fixed number.
	// We can use that number to find a bounding range within our rectangular interval.
	const float max_derivative_half_diagonal = 0.5f * max_derivative * Math_SQRT2;

	const real_t mid_x = 0.5 * (x.min + x.max);
	const real_t mid_y = 0.5 * (y.min + y.max);
	const float mid_value = noise_func(mid_x, mid_y);

	const real_t diag = Math::sqrt(math::squared(x.length()) + math::squared(y.length()));

	return math::Interval( //
			math::maxf(mid_value - max_derivative_half_diagonal * diag, -1),
			math::minf(mid_value + max_derivative_half_diagonal * diag, 1));
}

template <typename Noise_F>
inline math::Interval get_noise_range_3d(Noise_F noise_func, const math::Interval &x, const math::Interval &y,
		const math::Interval &z, float max_derivative) {
	const float max_derivative_half_diagonal = 0.5f * max_derivative * Math_SQRT2;

	const real_t mid_x = 0.5 * (x.min + x.max);
	const real_t mid_y = 0.5 * (y.min + y.max);
	const real_t mid_z = 0.5 * (z.min + z.max);
	const float mid_value = noise_func(mid_x, mid_y, mid_z);

	const real_t diag = Math::sqrt(math::squared(x.length()) + math::squared(y.length()) + math::squared(z.length()));

	return math::Interval( //
			math::maxf(mid_value - max_derivative_half_diagonal * diag, -1),
			math::minf(mid_value + max_derivative_half_diagonal * diag, 1));
}

} // namespace zylann

#endif // ZN_NOISE_RANGE_UTILITY_H
