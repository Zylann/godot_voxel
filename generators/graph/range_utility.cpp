#include "range_utility.h"
#include "../../util/math/sdf.h"
#include "../../util/noise/fast_noise_lite.h"

#include <core/image.h>
#include <modules/opensimplex/open_simplex_noise.h>
#include <scene/resources/curve.h>

// TODO We could skew max derivative estimation if the anchor is on a bump or a dip
// because in these cases, it becomes impossible for noise to go further up or further down

template <typename Noise_F>
inline Interval get_noise_range_2d(Noise_F noise_func, const Interval &x, const Interval &y, float max_derivative) {
	// Any unit vector away from a given evaluation point, the maximum difference is a fixed number.
	// We can use that number to find a bounding range within our rectangular interval.
	const float max_derivative_half_diagonal = 0.5f * max_derivative * Math_SQRT2;

	const float mid_x = 0.5 * (x.min + x.max);
	const float mid_y = 0.5 * (y.min + y.max);
	const float mid_value = noise_func(mid_x, mid_y);

	const float diag = Math::sqrt(squared(x.length()) + squared(y.length()));

	return Interval(
			::max(mid_value - max_derivative_half_diagonal * diag, -1.f),
			::min(mid_value + max_derivative_half_diagonal * diag, 1.f));
}

template <typename Noise_F>
inline Interval get_noise_range_3d(Noise_F noise_func, const Interval &x, const Interval &y, const Interval &z,
		float max_derivative) {
	const float max_derivative_half_diagonal = 0.5f * max_derivative * Math_SQRT2;

	const float mid_x = 0.5 * (x.min + x.max);
	const float mid_y = 0.5 * (y.min + y.max);
	const float mid_z = 0.5 * (z.min + z.max);
	const float mid_value = noise_func(mid_x, mid_y, mid_z);

	const float diag = Math::sqrt(squared(x.length()) + squared(y.length()) + squared(z.length()));

	return Interval(
			::max(mid_value - max_derivative_half_diagonal * diag, -1.f),
			::min(mid_value + max_derivative_half_diagonal * diag, 1.f));
}

Interval get_osn_octave_range_2d(OpenSimplexNoise *noise, const Interval &p_x, const Interval &p_y, int octave) {
	return get_noise_range_2d(
			[octave, noise](float x, float y) { return noise->_get_octave_noise_2d(octave, x, y); },
			p_x, p_y, 2.35f);
}

Interval get_osn_octave_range_3d(
		OpenSimplexNoise *noise, const Interval &p_x, const Interval &p_y, const Interval &p_z, int octave) {
	return get_noise_range_3d(
			[octave, noise](float x, float y, float z) { return noise->_get_octave_noise_3d(octave, x, y, z); },
			p_x, p_y, p_z, 2.5f);
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

void get_curve_monotonic_sections(Curve &curve, std::vector<CurveMonotonicSection> &sections) {
	const int res = curve.get_bake_resolution();
	float prev_y = curve.interpolate_baked(0.f);

	sections.clear();
	CurveMonotonicSection section;
	section.x_min = 0.f;
	section.y_min = curve.interpolate_baked(0.f);

	float prev_x = 0.f;
	bool current_stationary = true;
	bool current_increasing = false;

	for (int i = 1; i < res; ++i) {
		const float x = static_cast<float>(i) / res;
		const float y = curve.interpolate_baked(x);
		// Curve can sometimes appear flat but it still oscillates by very small amounts due to float imprecision
		// which occurred during bake(). Attempting to workaround that by taking the error into account
		const bool increasing = y > prev_y + CURVE_RANGE_MARGIN;
		const bool decreasing = y < prev_y - CURVE_RANGE_MARGIN;
		const bool stationary = increasing == false && decreasing == false;

		if (current_stationary) {
			current_stationary = stationary;
			current_increasing = increasing;

		} else if (i > 1 && !stationary && increasing != current_increasing) {
			section.x_max = prev_x;
			section.y_max = prev_y;
			sections.push_back(section);

			section.x_min = prev_x;
			section.y_min = prev_y;
			current_increasing = increasing;
			// Note, `current_stationary` does not become true ever again, because we only care about varying sections.
			// If a part of the curve becomes stationary, it will be included within the current section until it
			// starts increasing or decreasing.
		}

		prev_x = x;
		prev_y = y;
	}

	// Forcing 1 because the iteration doesn't go up to `res`
	section.x_max = 1.f;
	section.y_max = prev_y;
	sections.push_back(section);
}

Interval get_curve_range(Curve &curve, const std::vector<CurveMonotonicSection> &sections, Interval x) {
	// This implementation is linear. It assumes curves usually don't have many points.
	// If a curve has too many points, we may consider dynamically choosing a different algorithm.
	Interval y;
	unsigned int i = 0;
	if (x.min < sections[0].x_min) {
		// X range starts before the curve's minimum X
		y = Interval::from_single_value(curve.interpolate_baked(0.f));
	} else {
		// Find section from where the range starts
		for (; i < sections.size(); ++i) {
			const CurveMonotonicSection &section = sections[i];
			if (x.min >= section.x_min) {
				const float begin_y = curve.interpolate_baked(x.min);
				if (x.max < section.x_max) {
					// X range starts and ends in that section
					return Interval::from_unordered_values(begin_y, curve.interpolate_baked(x.max))
							.padded(CURVE_RANGE_MARGIN);
				} else {
					// X range starts in that section, and continues after it.
					// Will need to keep iterating, starting from here
					y = Interval::from_unordered_values(begin_y, curve.interpolate_baked(section.x_max));
					++i;
					break;
				}
			}
		}
	}
	for (; i < sections.size(); ++i) {
		const CurveMonotonicSection &section = sections[i];
		if (x.max >= section.x_max) {
			// X range covers this whole section and maybe more after it
			y.add_interval(Interval::from_unordered_values(section.y_min, section.y_max));
		} else {
			// X range ends in that section
			y.add_interval(Interval::from_unordered_values(section.y_min, curve.interpolate_baked(x.max)));
			break;
		}
	}
	return y.padded(CURVE_RANGE_MARGIN);
}

Interval get_curve_range(Curve &curve, bool &is_monotonic_increasing) {
	// TODO Would be nice to have the cache directly
	const int res = curve.get_bake_resolution();
	Interval range;
	float prev_v = curve.interpolate_baked(0.f);
	if (curve.interpolate_baked(1.f) > prev_v) {
		is_monotonic_increasing = true;
	}
	for (int i = 0; i < res; ++i) {
		const float v = curve.interpolate_baked(static_cast<float>(i) / res);
		range.add_point(v);
		if (v < prev_v) {
			is_monotonic_increasing = false;
		}
		prev_v = v;
	}
	return range;
}

Interval get_heightmap_range(Image &im) {
	return get_heightmap_range(im, Rect2i(0, 0, im.get_width(), im.get_height()));
}

Interval get_heightmap_range(Image &im, Rect2i rect) {
	ERR_FAIL_COND_V_MSG(
			im.is_compressed(), Interval(), String("Image format not supported: {0}").format(varray(im.get_format())));

	Interval r;

	im.lock();

	r.min = im.get_pixel(0, 0).r;
	r.max = r.min;

	const int max_x = rect.position.x + rect.size.x;
	const int max_y = rect.position.y + rect.size.y;

	for (int y = rect.position.y; y < max_y; ++y) {
		for (int x = rect.position.x; x < max_x; ++x) {
			r.add_point(im.get_pixel(x, y).r);
		}
	}

	im.unlock();

	return r;
}

SdfAffectingArguments sdf_subtract_side(Interval a, Interval b) {
	if (b.min > -a.min) {
		return SDF_ONLY_A;
	}
	if (b.max < -a.max) {
		return SDF_ONLY_B;
	}
	return SDF_BOTH;
}

SdfAffectingArguments sdf_polynomial_smooth_subtract_side(Interval a, Interval b, float s) {
	//     |  \  \  \        |
	//  ---1---x--x--x-------3--- b.max
	//     |    \  \  \      |
	//     |     \  \  \     |              (b)
	//     |      \  \  \    |               y
	//     |       \  \  \   |               |
	//  ---0--------x--x--x--2--- b.min      o---x (a)
	//     |         \s \ s\ |
	//    a.min             a.max

	if (b.min > -a.min + s) {
		return SDF_ONLY_B;
	}
	if (b.max < -a.max - s) {
		return SDF_ONLY_A;
	}
	return SDF_BOTH;
}

SdfAffectingArguments sdf_union_side(Interval a, Interval b) {
	if (a.max < b.min) {
		return SDF_ONLY_A;
	}
	if (b.max < a.min) {
		return SDF_ONLY_B;
	}
	return SDF_BOTH;
}

SdfAffectingArguments sdf_polynomial_smooth_union_side(Interval a, Interval b, float s) {
	if (a.max + s < b.min) {
		return SDF_ONLY_A;
	}
	if (b.max + s < a.min) {
		return SDF_ONLY_B;
	}
	return SDF_BOTH;
}

template <typename F>
inline Interval sdf_smooth_op(Interval b, Interval a, float s, F smooth_op_func) {
	// Smooth union and subtract are a generalization of `min(a, b)` and `max(-a, b)`, with a smooth junction.
	// That junction runs in a diagonal crossing zero (with equation `y = -x`).
	// Areas on the two sides of the junction are monotonic, i.e their derivatives should never cross zero,
	// because they are linear functions modified by a "smoothing" polynomial for which the tip is on diagonal.
	// So to find the output range, we can evaluate and sort the 4 corners,
	// and diagonal intersections if it crosses the area.

	//     |  \              |
	//  ---1---x-------------3--- b.max
	//     |    \            |
	//     |     \           |              (b)
	//     |      \          |               y
	//     |       \         |               |
	//  ---0--------x--------2--- b.min      o---x (a)
	//     |         \       |
	//    a.min             a.max

	const float v0 = smooth_op_func(b.min, a.min, s);
	const float v1 = smooth_op_func(b.max, a.min, s);
	const float v2 = smooth_op_func(b.min, a.max, s);
	const float v3 = smooth_op_func(b.max, a.max, s);

	const Vector2 diag_b_min(-b.min, b.min);
	const Vector2 diag_b_max(-b.max, b.max);
	const Vector2 diag_a_min(a.min, -a.min);
	const Vector2 diag_a_max(a.max, -a.max);

	const bool crossing_top = (diag_b_max.x > a.min && diag_b_max.x < a.max);
	const bool crossing_left = (diag_a_min.y > b.min && diag_a_min.y < b.max);

	if (crossing_left || crossing_top) {
		const bool crossing_right = (diag_a_max.y > b.min && diag_a_max.y < b.max);

		float v4;
		if (crossing_left) {
			v4 = smooth_op_func(diag_a_min.y, diag_a_min.x, s);
		} else {
			v4 = smooth_op_func(diag_b_max.y, diag_b_max.x, s);
		}

		float v5;
		if (crossing_right) {
			v5 = smooth_op_func(diag_a_max.y, diag_a_max.x, s);
		} else {
			v5 = smooth_op_func(diag_b_min.y, diag_b_min.x, s);
		}

		return Interval(min(v0, v1, v2, v3, v4, v5), max(v0, v1, v2, v3, v4, v5));
	}

	// The diagonal does not cross the area
	return Interval(min(v0, v1, v2, v3), max(v0, v1, v2, v3));
}

Interval sdf_smooth_union(Interval p_b, Interval p_a, float p_s) {
	// TODO Not tested
	// Had to use a lambda because otherwise it's ambiguous
	return sdf_smooth_op(p_b, p_a, p_s, [](float b, float a, float s) { return sdf_smooth_union(b, a, s); });
}

Interval sdf_smooth_subtract(Interval p_b, Interval p_a, float p_s) {
	return sdf_smooth_op(p_b, p_a, p_s, [](float b, float a, float s) { return sdf_smooth_subtract(b, a, s); });
}

static Interval get_fnl_cellular_value_range_2d(const FastNoiseLite *noise, Interval x, Interval y) {
	const float c0 = noise->get_noise_2d(x.min, y.min);
	const float c1 = noise->get_noise_2d(x.max, y.min);
	const float c2 = noise->get_noise_2d(x.min, y.max);
	const float c3 = noise->get_noise_2d(x.max, y.max);
	if (c0 == c1 && c1 == c2 && c2 == c3) {
		return Interval::from_single_value(c0);
	}
	return Interval{ -1, 1 };
}

static Interval get_fnl_cellular_value_range_3d(
		const fast_noise_lite::FastNoiseLite &fn, Interval x, Interval y, Interval z) {
	const float c0 = fn.GetNoise(x.min, y.min, z.min);
	const float c1 = fn.GetNoise(x.max, y.min, z.min);
	const float c2 = fn.GetNoise(x.min, y.max, z.min);
	const float c3 = fn.GetNoise(x.max, y.max, z.min);
	const float c4 = fn.GetNoise(x.max, y.max, z.max);
	const float c5 = fn.GetNoise(x.max, y.max, z.max);
	const float c6 = fn.GetNoise(x.max, y.max, z.max);
	const float c7 = fn.GetNoise(x.max, y.max, z.max);
	if (c0 == c1 && c1 == c2 && c2 == c3 && c3 == c4 && c4 == c5 && c5 == c6 && c6 == c7) {
		return Interval::from_single_value(c0);
	}
	return Interval{ -1, 1 };
}

static Interval get_fnl_cellular_range(const FastNoiseLite *noise) {
	// There are many combinations with Cellular noise so instead of implementing them with intervals,
	// I used empiric tests to figure out some bounds.

	// Value mode must be handled separately.

	switch (noise->get_cellular_distance_function()) {
		case FastNoiseLite::CELLULAR_DISTANCE_EUCLIDEAN:
			switch (noise->get_cellular_return_type()) {
				case FastNoiseLite::CELLULAR_RETURN_DISTANCE:
					return Interval{ -1.f, 0.08f };
				case FastNoiseLite::CELLULAR_RETURN_DISTANCE_2:
					return Interval{ -0.92f, 0.35 };
				case FastNoiseLite::CELLULAR_RETURN_DISTANCE_2_ADD:
					return Interval{ -0.92f, 0.1 };
				case FastNoiseLite::CELLULAR_RETURN_DISTANCE_2_SUB:
					return Interval{ -1, 0.15 };
				case FastNoiseLite::CELLULAR_RETURN_DISTANCE_2_MUL:
					return Interval{ -1, 0 };
				case FastNoiseLite::CELLULAR_RETURN_DISTANCE_2_DIV:
					return Interval{ -1, 0 };
				default:
					ERR_FAIL_V(Interval(-1, 1));
			}
			break;

		case FastNoiseLite::CELLULAR_DISTANCE_EUCLIDEAN_SQ:
			switch (noise->get_cellular_return_type()) {
				case FastNoiseLite::CELLULAR_RETURN_DISTANCE:
					return Interval{ -1, 0.2 };
				case FastNoiseLite::CELLULAR_RETURN_DISTANCE_2:
					return Interval{ -1, 0.8 };
				case FastNoiseLite::CELLULAR_RETURN_DISTANCE_2_ADD:
					return Interval{ -1, 0.2 };
				case FastNoiseLite::CELLULAR_RETURN_DISTANCE_2_SUB:
					return Interval{ -1, 0.7 };
				case FastNoiseLite::CELLULAR_RETURN_DISTANCE_2_MUL:
					return Interval{ -1, 0 };
				case FastNoiseLite::CELLULAR_RETURN_DISTANCE_2_DIV:
					return Interval{ -1, 0 };
				default:
					ERR_FAIL_V(Interval(-1, 1));
			}

		case FastNoiseLite::CELLULAR_DISTANCE_MANHATTAN:
			switch (noise->get_cellular_return_type()) {
				case FastNoiseLite::CELLULAR_RETURN_DISTANCE:
					return Interval{ -1, 0.75 };
				case FastNoiseLite::CELLULAR_RETURN_DISTANCE_2:
					return Interval{ -0.9, 0.8 };
				case FastNoiseLite::CELLULAR_RETURN_DISTANCE_2_ADD:
					return Interval{ -0.8, 0.8 };
				case FastNoiseLite::CELLULAR_RETURN_DISTANCE_2_SUB:
					return Interval{ -1.0, 0.5 };
				case FastNoiseLite::CELLULAR_RETURN_DISTANCE_2_MUL:
					return Interval{ -1.0, 0.7 };
				case FastNoiseLite::CELLULAR_RETURN_DISTANCE_2_DIV:
					return Interval{ -1.0, 0.0 };
				default:
					ERR_FAIL_V(Interval(-1, 1));
			}

		case FastNoiseLite::CELLULAR_DISTANCE_HYBRID:
			switch (noise->get_cellular_return_type()) {
				case FastNoiseLite::CELLULAR_RETURN_DISTANCE:
					return Interval{ -1, 1.75 };
				case FastNoiseLite::CELLULAR_RETURN_DISTANCE_2:
					return Interval{ -0.9, 2.3 };
				case FastNoiseLite::CELLULAR_RETURN_DISTANCE_2_ADD:
					return Interval{ -0.9, 1.9 };
				case FastNoiseLite::CELLULAR_RETURN_DISTANCE_2_SUB:
					return Interval{ -1.0, 1.85 };
				case FastNoiseLite::CELLULAR_RETURN_DISTANCE_2_MUL:
					return Interval{ -1.0, 3.4 };
				case FastNoiseLite::CELLULAR_RETURN_DISTANCE_2_DIV:
					return Interval{ -1.0, 0.0 };
				default:
					ERR_FAIL_V(Interval(-1, 1));
			}
	}
	return Interval{ -1.f, 1.f };
}

void fnl_transform_noise_coordinate(const fast_noise_lite::FastNoiseLite &fn, Interval &x, Interval &y, Interval &z) {
	// Same logic as in the FastNoiseLite internal function

	x *= fn.mFrequency;
	y *= fn.mFrequency;
	z *= fn.mFrequency;

	switch (fn.mTransformType3D) {
		case fast_noise_lite::FastNoiseLite::TransformType3D_ImproveXYPlanes: {
			Interval xy = x + y;
			Interval s2 = xy * (-0.211324865405187);
			z *= 0.577350269189626;
			x += s2 - z;
			y = y + s2 - z;
			z += xy * 0.577350269189626;
		} break;
		case fast_noise_lite::FastNoiseLite::TransformType3D_ImproveXZPlanes: {
			Interval xz = x + z;
			Interval s2 = xz * (-0.211324865405187);
			y *= 0.577350269189626;
			x += s2 - y;
			z += s2 - y;
			y += xz * 0.577350269189626;
		} break;
		case fast_noise_lite::FastNoiseLite::TransformType3D_DefaultOpenSimplex2: {
			const float R3 = (2.0 / 3.0);
			Interval r = (x + y + z) * R3; // Rotation, not skew
			x = r - x;
			y = r - y;
			z = r - z;
		} break;
		default:
			break;
	}
}

Interval fnl_single_opensimplex2(const fast_noise_lite::FastNoiseLite &fn, int seed, Interval p_x, Interval p_y,
		Interval p_z) {
	// According to OpenSimplex2 author, the 3D version is supposed to have a max derivative around 4.23718
	// https://www.wolframalpha.com/input/?i=max+d%2Fdx+32.69428253173828125+*+x+*+%28%280.6-x%5E2%29%5E4%29+from+-0.6+to+0.6
	// But empiric measures have shown it around 8. Discontinuities do exist in this noise though,
	// which makes this measuring harder
	return get_noise_range_3d(
			[&fn, seed](float x, float y, float z) { return fn.SingleOpenSimplex2(seed, x, y, z); },
			p_x, p_y, p_z, 4.23718f);
}

Interval fnl_single_opensimplex2s(const fast_noise_lite::FastNoiseLite &fn, int seed, Interval p_x, Interval p_y,
		Interval p_z) {
	return get_noise_range_3d(
			[&fn, seed](float x, float y, float z) { return fn.SingleOpenSimplex2(seed, x, y, z); },
			// Max derivative found from empiric tests
			p_x, p_y, p_z, 2.5f);
}

Interval fnl_single_cellular(const FastNoiseLite *noise, Interval x, Interval y, Interval z) {
	const fast_noise_lite::FastNoiseLite &fn = noise->get_noise_internal();
	if (fn.mCellularReturnType == fast_noise_lite::FastNoiseLite::CellularReturnType_CellValue) {
		return get_fnl_cellular_value_range_3d(fn, x, y, z);
	}
	return get_fnl_cellular_range(noise);
}

Interval fnl_single_perlin(const fast_noise_lite::FastNoiseLite &fn, int seed, Interval p_x, Interval p_y,
		Interval p_z) {
	return get_noise_range_3d(
			[&fn, seed](float x, float y, float z) { return fn.SinglePerlin(seed, x, y, z); },
			// Max derivative found from empiric tests
			p_x, p_y, p_z, 3.2f);
}

Interval fnl_single_value_cubic(const fast_noise_lite::FastNoiseLite &fn, int seed, Interval p_x, Interval p_y,
		Interval p_z) {
	return get_noise_range_3d(
			[&fn, seed](float x, float y, float z) { return fn.SingleValueCubic(seed, x, y, z); },
			// Max derivative found from empiric tests
			p_x, p_y, p_z, 1.2f);
}

Interval fnl_single_value(const fast_noise_lite::FastNoiseLite &fn, int seed, Interval p_x, Interval p_y,
		Interval p_z) {
	return get_noise_range_3d(
			[&fn, seed](float x, float y, float z) { return fn.SingleValue(seed, x, y, z); },
			// Max derivative found from empiric tests
			p_x, p_y, p_z, 3.0f);
}

Interval fnl_gen_noise_single(const FastNoiseLite *noise, int seed, Interval x, Interval y, Interval z) {
	// Same logic as in the FastNoiseLite internal function
	const fast_noise_lite::FastNoiseLite &fn = noise->get_noise_internal();

	switch (fn.mNoiseType) {
		case fast_noise_lite::FastNoiseLite::NoiseType_OpenSimplex2:
			return fnl_single_opensimplex2(fn, seed, x, y, z);
		case fast_noise_lite::FastNoiseLite::NoiseType_OpenSimplex2S:
			return fnl_single_opensimplex2s(fn, seed, x, y, z);
		case fast_noise_lite::FastNoiseLite::NoiseType_Cellular:
			return fnl_single_cellular(noise, x, y, z);
		case fast_noise_lite::FastNoiseLite::NoiseType_Perlin:
			return fnl_single_perlin(fn, seed, x, y, z);
		case fast_noise_lite::FastNoiseLite::NoiseType_ValueCubic:
			return fnl_single_value_cubic(fn, seed, x, y, z);
		case fast_noise_lite::FastNoiseLite::NoiseType_Value:
			return fnl_single_value(fn, seed, x, y, z);
		default:
			return Interval::from_single_value(0);
	}
}

Interval fnl_gen_fractal_fbm(const FastNoiseLite *p_noise, Interval x, Interval y, Interval z) {
	// Same logic as in the FastNoiseLite internal function
	const fast_noise_lite::FastNoiseLite &fn = p_noise->get_noise_internal();

	int seed = fn.mSeed;
	Interval sum;
	Interval amp = Interval::from_single_value(fn.mFractalBounding);

	for (int i = 0; i < fn.mOctaves; i++) {
		Interval noise = fnl_gen_noise_single(p_noise, seed++, x, y, z);
		sum += noise * amp;
		amp *= lerp(
				Interval::from_single_value(1.0f),
				(noise + Interval::from_single_value(1.0f)) * 0.5f,
				Interval::from_single_value(fn.mWeightedStrength));

		x *= fn.mLacunarity;
		y *= fn.mLacunarity;
		z *= fn.mLacunarity;
		amp *= fn.mGain;
	}

	return sum;
}

Interval fnl_gen_fractal_ridged(const FastNoiseLite *p_noise, Interval x, Interval y, Interval z) {
	// Same logic as in the FastNoiseLite internal function
	const fast_noise_lite::FastNoiseLite &fn = p_noise->get_noise_internal();

	int seed = fn.mSeed;
	Interval sum;
	Interval amp = Interval::from_single_value(fn.mFractalBounding);

	for (int i = 0; i < fn.mOctaves; i++) {
		Interval noise = abs(fnl_gen_noise_single(p_noise, seed++, x, y, z));
		sum += (noise * -2 + 1) * amp;
		amp *= lerp(
				Interval::from_single_value(1.0f),
				Interval::from_single_value(1.0f) - noise,
				Interval::from_single_value(fn.mWeightedStrength));

		x *= fn.mLacunarity;
		y *= fn.mLacunarity;
		z *= fn.mLacunarity;
		amp *= fn.mGain;
	}

	return sum;
}

Interval fnl_get_noise(const FastNoiseLite *noise, Interval x, Interval y, Interval z) {
	// Same logic as in the FastNoiseLite internal function
	const fast_noise_lite::FastNoiseLite &fn = noise->get_noise_internal();

	fnl_transform_noise_coordinate(fn, x, y, z);

	switch (noise->get_fractal_type()) {
		default:
			return fnl_gen_noise_single(noise, noise->get_seed(), x, y, z);
		case FastNoiseLite::FRACTAL_FBM:
			return fnl_gen_fractal_fbm(noise, x, y, z);
		case FastNoiseLite::FRACTAL_RIDGED:
			return fnl_gen_fractal_ridged(noise, x, y, z);
		case FastNoiseLite::FRACTAL_PING_PONG:
			// TODO Ping pong
			return Interval(-1.f, 1.f);
	}
}

Interval get_fnl_range_2d(const FastNoiseLite *noise, Interval x, Interval y) {
	// TODO More precise analysis using derivatives
	// TODO Take warp noise into account
	switch (noise->get_noise_type()) {
		case FastNoiseLite::TYPE_CELLULAR:
			if (noise->get_cellular_return_type() == FastNoiseLite::CELLULAR_RETURN_CELL_VALUE) {
				return get_fnl_cellular_value_range_2d(noise, x, y);
			}
			return get_fnl_cellular_range(noise);
		default:
			return Interval{ -1.f, 1.f };
	}
}

Interval get_fnl_range_3d(const FastNoiseLite *noise, Interval x, Interval y, Interval z) {
	if (noise->get_warp_noise().is_null()) {
		return fnl_get_noise(noise, x, y, z);
	}
	// TODO Take warp noise into account
	switch (noise->get_noise_type()) {
		case FastNoiseLite::TYPE_CELLULAR:
			if (noise->get_cellular_return_type() == FastNoiseLite::CELLULAR_RETURN_CELL_VALUE) {
				return get_fnl_cellular_value_range_3d(noise->get_noise_internal(), x, y, z);
			}
			return get_fnl_cellular_range(noise);
		default:
			return Interval{ -1.f, 1.f };
	}
}

Interval2 get_fnl_gradient_range_2d(const FastNoiseLiteGradient *noise, Interval x, Interval y) {
	// TODO More precise analysis
	const float amp = Math::abs(noise->get_amplitude());
	return Interval2{
		Interval{ x.min - amp, x.max + amp },
		Interval{ y.min - amp, y.max + amp }
	};
}

Interval3 get_fnl_gradient_range_3d(const FastNoiseLiteGradient *noise, Interval x, Interval y, Interval z) {
	// TODO More precise analysis
	const float amp = Math::abs(noise->get_amplitude());
	return Interval3{
		Interval{ x.min - amp, x.max + amp },
		Interval{ y.min - amp, y.max + amp },
		Interval{ z.min - amp, z.max + amp }
	};
}
