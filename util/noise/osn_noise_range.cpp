#include "osn_noise_range.h"
#include "noise_range_utility.h"
#include <modules/opensimplex/open_simplex_noise.h>

namespace zylann {

using namespace math;

Interval get_osn_octave_range_2d(const OpenSimplexNoise &noise, const Interval &p_x, const Interval &p_y, int octave) {
	return get_noise_range_2d(
			[octave, &noise](real_t x, real_t y) { //
				return noise._get_octave_noise_2d(octave, x, y);
			},
			p_x, p_y, 2.35f);
}

Interval get_osn_octave_range_3d(
		const OpenSimplexNoise &noise, const Interval &p_x, const Interval &p_y, const Interval &p_z, int octave) {
	return get_noise_range_3d(
			[octave, &noise](real_t x, real_t y, real_t z) { //
				return noise._get_octave_noise_3d(octave, x, y, z);
			},
			p_x, p_y, p_z, 2.5f);
}

Interval get_osn_range_2d(const OpenSimplexNoise &noise, Interval x, Interval y) {
	// Same implementation as `get_noise_2d`

	if (x.is_single_value() && y.is_single_value()) {
		return Interval::from_single_value(noise.get_noise_2d(x.min, y.min));
	}

	x /= noise.get_period();
	y /= noise.get_period();

	float amp = 1.0;
	float max = 1.0;
	Interval sum = get_osn_octave_range_2d(noise, x, y, 0);

	int i = 0;
	while (++i < noise.get_octaves()) {
		x *= noise.get_lacunarity();
		y *= noise.get_lacunarity();
		amp *= noise.get_persistence();
		max += amp;
		sum += get_osn_octave_range_2d(noise, x, y, i) * amp;
	}

	return sum / max;
}

Interval get_osn_range_3d(const OpenSimplexNoise &noise, Interval x, Interval y, Interval z) {
	// Same implementation as `get_noise_3d`

	if (x.is_single_value() && y.is_single_value()) {
		return Interval::from_single_value(noise.get_noise_2d(x.min, y.min));
	}

	x /= noise.get_period();
	y /= noise.get_period();
	z /= noise.get_period();

	float amp = 1.0;
	float max = 1.0;
	Interval sum = get_osn_octave_range_3d(noise, x, y, z, 0);

	int i = 0;
	while (++i < noise.get_octaves()) {
		x *= noise.get_lacunarity();
		y *= noise.get_lacunarity();
		z *= noise.get_lacunarity();
		amp *= noise.get_persistence();
		max += amp;
		sum += get_osn_octave_range_3d(noise, x, y, z, i) * amp;
	}

	return sum / max;
}

} // namespace zylann
