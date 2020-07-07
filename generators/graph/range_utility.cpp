#include "range_utility.h"
#include "../../util/utility.h"

Interval get_osn_octave_range_2d(OpenSimplexNoise *noise, const Interval &x, const Interval &y, int octave) {
	// Any unit vector away from a given evaluation point, the maximum difference is a fixed number.
	// We can use that number to find a bounding range within our rectangular interval.
	static const float max_derivative = 2.35;
	static const float max_derivative_half_diagonal = 0.5f * max_derivative * Math_SQRT2;

	float mid_x = 0.5 * (x.min + x.max);
	float mid_y = 0.5 * (y.min + y.max);
	float mid_value = noise->_get_octave_noise_2d(octave, mid_x, mid_y);

	float diag = Math::sqrt(squared(x.length()) + squared(y.length()));

	return Interval(
			::max(mid_value - max_derivative_half_diagonal * diag, -1.f),
			::min(mid_value + max_derivative_half_diagonal * diag, 1.f));
}

Interval get_osn_octave_range_3d(OpenSimplexNoise *noise, const Interval &x, const Interval &y, const Interval &z, int octave) {
	// Any unit vector away from a given evaluation point, the maximum difference is a fixed number.
	// We can use that number to find a bounding range within our box interval.
	static const float max_derivative = 2.5;
	static const float max_derivative_half_diagonal = 0.5f * max_derivative * Math_SQRT2;

	float mid_x = 0.5 * (x.min + x.max);
	float mid_y = 0.5 * (y.min + y.max);
	float mid_z = 0.5 * (z.min + z.max);
	float mid_value = noise->_get_octave_noise_3d(octave, mid_x, mid_y, mid_z);

	float diag = Math::sqrt(squared(x.length()) + squared(y.length()) + squared(z.length()));

	return Interval(
			::max(mid_value - max_derivative_half_diagonal * diag, -1.f),
			::min(mid_value + max_derivative_half_diagonal * diag, 1.f));
}

Interval get_osn_range_2d(OpenSimplexNoise *noise, Interval x, Interval y) {
	// Same implementation as `get_noise_2d`

	if (x.is_single_value() && y.is_single_value()) {
		return Interval::from_single_value(noise->get_noise_2d(x.min, y.min));
	}

	x /= noise->get_period();
	y /= noise->get_period();

	float amp = 1.0;
	float max = 1.0;
	Interval sum = get_osn_octave_range_2d(noise, x, y, 0);

	int i = 0;
	while (++i < noise->get_octaves()) {
		x *= noise->get_lacunarity();
		y *= noise->get_lacunarity();
		amp *= noise->get_persistence();
		max += amp;
		sum += get_osn_octave_range_2d(noise, x, y, i) * amp;
	}

	return sum / max;
}

Interval get_osn_range_3d(OpenSimplexNoise *noise, Interval x, Interval y, Interval z) {
	// Same implementation as `get_noise_3d`

	if (x.is_single_value() && y.is_single_value()) {
		return Interval::from_single_value(noise->get_noise_2d(x.min, y.min));
	}

	x /= noise->get_period();
	y /= noise->get_period();
	z /= noise->get_period();

	float amp = 1.0;
	float max = 1.0;
	Interval sum = get_osn_octave_range_3d(noise, x, y, z, 0);

	int i = 0;
	while (++i < noise->get_octaves()) {
		x *= noise->get_lacunarity();
		y *= noise->get_lacunarity();
		z *= noise->get_lacunarity();
		amp *= noise->get_persistence();
		max += amp;
		sum += get_osn_octave_range_3d(noise, x, y, z, i) * amp;
	}

	return sum / max;
}

Interval get_curve_range(Curve &curve, uint8_t &is_monotonic_increasing) {
	// TODO Would be nice to have the cache directly
	const int res = curve.get_bake_resolution();
	Interval range;
	float prev_v = curve.interpolate_baked(0.f);
	if (curve.interpolate_baked(1.f) > prev_v) {
		is_monotonic_increasing = 1;
	}
	for (int i = 0; i < res; ++i) {
		const float v = curve.interpolate_baked(static_cast<float>(i) / res);
		range.add_point(v);
		if (v < prev_v) {
			is_monotonic_increasing = 0;
		}
		prev_v = v;
	}
	return range;
}

Interval get_heightmap_range(Image &im) {
	switch (im.get_format()) {
		case Image::FORMAT_R8:
		case Image::FORMAT_RG8:
		case Image::FORMAT_RGB8:
		case Image::FORMAT_RGBA8:
		case Image::FORMAT_RH:
		case Image::FORMAT_RGH:
		case Image::FORMAT_RGBH:
		case Image::FORMAT_RGBAH:
		case Image::FORMAT_RF:
		case Image::FORMAT_RGF:
		case Image::FORMAT_RGBF:
		case Image::FORMAT_RGBAF: {
			Interval r;
			im.lock();
			r.min = im.get_pixel(0, 0).r;
			r.max = r.min;
			for (int y = 0; y < im.get_height(); ++y) {
				for (int x = 0; x < im.get_width(); ++x) {
					r.add_point(im.get_pixel(x, y).r);
				}
			}
			im.unlock();
			return r;
		} break;

		default:
			ERR_FAIL_V_MSG(Interval(), "Image format not supported");
			break;
	}
	return Interval();
}

#ifdef DEBUG_ENABLED
void test_osn_max_derivative() {
	// Empiric test to find the maximum derivative of OpenSimplex.
	// The author of the algorithm told me it should be 5.2 for OpenSimplex2S,
	// but in Godot we have an older version so it may be different

	osn_context ctx;
	open_simplex_noise(131183, &ctx);
	static const int iterations = 1000000;

	for (int j = 0; j < 50; ++j) {

		double max_derivative_2d = 0.0;
		double max_derivative_3d = 0.0;
		double step = Math::lerp(0.0001, 0.001, static_cast<double>(j) / 50.0);

		double n0, n1, d;

		for (int i = 0; i < iterations; ++i) {
			double xc = Math::randd() * 1000.0;
			double yc = Math::randd() * 1000.0;
			double zc = Math::randd() * 1000.0;

			n0 = open_simplex_noise2(&ctx, xc, yc);
			n1 = open_simplex_noise2(&ctx, xc + step, yc);
			d = fabs(n1 - n0);
			if (d > max_derivative_2d) {
				max_derivative_2d = d;
			}

			n0 = open_simplex_noise3(&ctx, xc, yc, zc);
			n1 = open_simplex_noise3(&ctx, xc + step, yc, zc);
			d = fabs(n1 - n0);
			if (d > max_derivative_3d) {
				max_derivative_3d = d;
			}
		}

		max_derivative_2d /= step;
		max_derivative_3d /= step;
		print_line(String("{0}, {1}, {2}").format(varray(step, max_derivative_2d, max_derivative_3d)));
	}
}
#endif
