#include "../noise_range_utility.h"
#include "fast_noise_lite.h"
#include "fast_noise_lite_gradient.h"

namespace zylann {

using namespace math;

static Interval get_fnl_cellular_value_range_2d(const ZN_FastNoiseLite &noise, Interval x, Interval y) {
	const float c0 = noise.get_noise_2d(x.min, y.min);
	const float c1 = noise.get_noise_2d(x.max, y.min);
	const float c2 = noise.get_noise_2d(x.min, y.max);
	const float c3 = noise.get_noise_2d(x.max, y.max);
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

static Interval get_fnl_cellular_range(const ZN_FastNoiseLite &noise) {
	// There are many combinations with Cellular noise so instead of implementing them with intervals,
	// I used empiric tests to figure out some bounds.

	// Value mode must be handled separately.

	switch (noise.get_cellular_distance_function()) {
		case ZN_FastNoiseLite::CELLULAR_DISTANCE_EUCLIDEAN:
			switch (noise.get_cellular_return_type()) {
				case ZN_FastNoiseLite::CELLULAR_RETURN_DISTANCE:
					return Interval{ -1.f, 0.08f };
				case ZN_FastNoiseLite::CELLULAR_RETURN_DISTANCE_2:
					return Interval{ -0.92f, 0.35 };
				case ZN_FastNoiseLite::CELLULAR_RETURN_DISTANCE_2_ADD:
					return Interval{ -0.92f, 0.1 };
				case ZN_FastNoiseLite::CELLULAR_RETURN_DISTANCE_2_SUB:
					return Interval{ -1, 0.15 };
				case ZN_FastNoiseLite::CELLULAR_RETURN_DISTANCE_2_MUL:
					return Interval{ -1, 0 };
				case ZN_FastNoiseLite::CELLULAR_RETURN_DISTANCE_2_DIV:
					return Interval{ -1, 0 };
				default:
					ERR_FAIL_V(Interval(-1, 1));
			}
			break;

		case ZN_FastNoiseLite::CELLULAR_DISTANCE_EUCLIDEAN_SQ:
			switch (noise.get_cellular_return_type()) {
				case ZN_FastNoiseLite::CELLULAR_RETURN_DISTANCE:
					return Interval{ -1, 0.2 };
				case ZN_FastNoiseLite::CELLULAR_RETURN_DISTANCE_2:
					return Interval{ -1, 0.8 };
				case ZN_FastNoiseLite::CELLULAR_RETURN_DISTANCE_2_ADD:
					return Interval{ -1, 0.2 };
				case ZN_FastNoiseLite::CELLULAR_RETURN_DISTANCE_2_SUB:
					return Interval{ -1, 0.7 };
				case ZN_FastNoiseLite::CELLULAR_RETURN_DISTANCE_2_MUL:
					return Interval{ -1, 0 };
				case ZN_FastNoiseLite::CELLULAR_RETURN_DISTANCE_2_DIV:
					return Interval{ -1, 0 };
				default:
					ERR_FAIL_V(Interval(-1, 1));
			}

		case ZN_FastNoiseLite::CELLULAR_DISTANCE_MANHATTAN:
			switch (noise.get_cellular_return_type()) {
				case ZN_FastNoiseLite::CELLULAR_RETURN_DISTANCE:
					return Interval{ -1, 0.75 };
				case ZN_FastNoiseLite::CELLULAR_RETURN_DISTANCE_2:
					return Interval{ -0.9, 0.8 };
				case ZN_FastNoiseLite::CELLULAR_RETURN_DISTANCE_2_ADD:
					return Interval{ -0.8, 0.8 };
				case ZN_FastNoiseLite::CELLULAR_RETURN_DISTANCE_2_SUB:
					return Interval{ -1.0, 0.5 };
				case ZN_FastNoiseLite::CELLULAR_RETURN_DISTANCE_2_MUL:
					return Interval{ -1.0, 0.7 };
				case ZN_FastNoiseLite::CELLULAR_RETURN_DISTANCE_2_DIV:
					return Interval{ -1.0, 0.0 };
				default:
					ERR_FAIL_V(Interval(-1, 1));
			}

		case ZN_FastNoiseLite::CELLULAR_DISTANCE_HYBRID:
			switch (noise.get_cellular_return_type()) {
				case ZN_FastNoiseLite::CELLULAR_RETURN_DISTANCE:
					return Interval{ -1, 1.75 };
				case ZN_FastNoiseLite::CELLULAR_RETURN_DISTANCE_2:
					return Interval{ -0.9, 2.3 };
				case ZN_FastNoiseLite::CELLULAR_RETURN_DISTANCE_2_ADD:
					return Interval{ -0.9, 1.9 };
				case ZN_FastNoiseLite::CELLULAR_RETURN_DISTANCE_2_SUB:
					return Interval{ -1.0, 1.85 };
				case ZN_FastNoiseLite::CELLULAR_RETURN_DISTANCE_2_MUL:
					return Interval{ -1.0, 3.4 };
				case ZN_FastNoiseLite::CELLULAR_RETURN_DISTANCE_2_DIV:
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

Interval fnl_single_opensimplex2(
		const fast_noise_lite::FastNoiseLite &fn, int seed, Interval p_x, Interval p_y, Interval p_z) {
	// According to OpenSimplex2 author, the 3D version is supposed to have a max derivative around 4.23718
	// https://www.wolframalpha.com/input/?i=max+d%2Fdx+32.69428253173828125+*+x+*+%28%280.6-x%5E2%29%5E4%29+from+-0.6+to+0.6
	// But empiric measures have shown it around 8. Discontinuities do exist in this noise though,
	// which makes this measuring harder
	return get_noise_range_3d(
			[&fn, seed](real_t x, real_t y, real_t z) { //
				return fn.SingleOpenSimplex2(seed, x, y, z);
			},
			p_x, p_y, p_z, 4.23718f);
}

Interval fnl_single_opensimplex2s(
		const fast_noise_lite::FastNoiseLite &fn, int seed, Interval p_x, Interval p_y, Interval p_z) {
	return get_noise_range_3d(
			[&fn, seed](real_t x, real_t y, real_t z) { //
				return fn.SingleOpenSimplex2(seed, x, y, z);
			},
			// Max derivative found from empiric tests
			p_x, p_y, p_z, 2.5f);
}

Interval fnl_single_cellular(const ZN_FastNoiseLite &noise, Interval x, Interval y, Interval z) {
	const fast_noise_lite::FastNoiseLite &fn = noise.get_noise_internal();
	if (fn.mCellularReturnType == fast_noise_lite::FastNoiseLite::CellularReturnType_CellValue) {
		return get_fnl_cellular_value_range_3d(fn, x, y, z);
	}
	return get_fnl_cellular_range(noise);
}

Interval fnl_single_perlin(
		const fast_noise_lite::FastNoiseLite &fn, int seed, Interval p_x, Interval p_y, Interval p_z) {
	return get_noise_range_3d(
			[&fn, seed](real_t x, real_t y, real_t z) { //
				return fn.SinglePerlin(seed, x, y, z);
			},
			// Max derivative found from empiric tests
			p_x, p_y, p_z, 3.2f);
}

Interval fnl_single_value_cubic(
		const fast_noise_lite::FastNoiseLite &fn, int seed, Interval p_x, Interval p_y, Interval p_z) {
	return get_noise_range_3d(
			[&fn, seed](real_t x, real_t y, real_t z) { //
				return fn.SingleValueCubic(seed, x, y, z);
			},
			// Max derivative found from empiric tests
			p_x, p_y, p_z, 1.2f);
}

Interval fnl_single_value(
		const fast_noise_lite::FastNoiseLite &fn, int seed, Interval p_x, Interval p_y, Interval p_z) {
	return get_noise_range_3d(
			[&fn, seed](real_t x, real_t y, real_t z) { //
				return fn.SingleValue(seed, x, y, z);
			},
			// Max derivative found from empiric tests
			p_x, p_y, p_z, 3.0f);
}

Interval fnl_gen_noise_single(const ZN_FastNoiseLite &noise, int seed, Interval x, Interval y, Interval z) {
	// Same logic as in the FastNoiseLite internal function
	const fast_noise_lite::FastNoiseLite &fn = noise.get_noise_internal();

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

Interval fnl_gen_fractal_fbm(const ZN_FastNoiseLite &p_noise, Interval x, Interval y, Interval z) {
	// Same logic as in the FastNoiseLite internal function
	const fast_noise_lite::FastNoiseLite &fn = p_noise.get_noise_internal();

	int seed = fn.mSeed;
	Interval sum;
	Interval amp = Interval::from_single_value(fn.mFractalBounding);

	for (int i = 0; i < fn.mOctaves; i++) {
		Interval noise = fnl_gen_noise_single(p_noise, seed++, x, y, z);
		sum += noise * amp;
		amp *= lerp(Interval::from_single_value(1.0f), (noise + Interval::from_single_value(1.0f)) * 0.5f,
				Interval::from_single_value(fn.mWeightedStrength));

		x *= fn.mLacunarity;
		y *= fn.mLacunarity;
		z *= fn.mLacunarity;
		amp *= fn.mGain;
	}

	return sum;
}

Interval fnl_gen_fractal_ridged(const ZN_FastNoiseLite &p_noise, Interval x, Interval y, Interval z) {
	// Same logic as in the FastNoiseLite internal function
	const fast_noise_lite::FastNoiseLite &fn = p_noise.get_noise_internal();

	int seed = fn.mSeed;
	Interval sum;
	Interval amp = Interval::from_single_value(fn.mFractalBounding);

	for (int i = 0; i < fn.mOctaves; i++) {
		Interval noise = abs(fnl_gen_noise_single(p_noise, seed++, x, y, z));
		sum += (noise * -2 + 1) * amp;
		amp *= lerp(Interval::from_single_value(1.0f), Interval::from_single_value(1.0f) - noise,
				Interval::from_single_value(fn.mWeightedStrength));

		x *= fn.mLacunarity;
		y *= fn.mLacunarity;
		z *= fn.mLacunarity;
		amp *= fn.mGain;
	}

	return sum;
}

Interval fnl_get_noise(const ZN_FastNoiseLite &noise, Interval x, Interval y, Interval z) {
	// Same logic as in the FastNoiseLite internal function
	const fast_noise_lite::FastNoiseLite &fn = noise.get_noise_internal();

	fnl_transform_noise_coordinate(fn, x, y, z);

	switch (noise.get_fractal_type()) {
		default:
			return fnl_gen_noise_single(noise, noise.get_seed(), x, y, z);
		case ZN_FastNoiseLite::FRACTAL_FBM:
			return fnl_gen_fractal_fbm(noise, x, y, z);
		case ZN_FastNoiseLite::FRACTAL_RIDGED:
			return fnl_gen_fractal_ridged(noise, x, y, z);
		case ZN_FastNoiseLite::FRACTAL_PING_PONG:
			// TODO Ping pong
			return Interval(-1.f, 1.f);
	}
}

Interval get_fnl_range_2d(const ZN_FastNoiseLite &noise, Interval x, Interval y) {
	// TODO More precise analysis using derivatives
	// TODO Take warp noise into account
	switch (noise.get_noise_type()) {
		case ZN_FastNoiseLite::TYPE_CELLULAR:
			if (noise.get_cellular_return_type() == ZN_FastNoiseLite::CELLULAR_RETURN_CELL_VALUE) {
				return get_fnl_cellular_value_range_2d(noise, x, y);
			}
			return get_fnl_cellular_range(noise);
		default:
			return Interval{ -1.f, 1.f };
	}
}

Interval get_fnl_range_3d(const ZN_FastNoiseLite &noise, Interval x, Interval y, Interval z) {
	if (noise.get_warp_noise().is_null()) {
		return fnl_get_noise(noise, x, y, z);
	}
	// TODO Take warp noise into account
	switch (noise.get_noise_type()) {
		case ZN_FastNoiseLite::TYPE_CELLULAR:
			if (noise.get_cellular_return_type() == ZN_FastNoiseLite::CELLULAR_RETURN_CELL_VALUE) {
				return get_fnl_cellular_value_range_3d(noise.get_noise_internal(), x, y, z);
			}
			return get_fnl_cellular_range(noise);
		default:
			return Interval{ -1.f, 1.f };
	}
}

math::Interval2 get_fnl_gradient_range_2d(const ZN_FastNoiseLiteGradient &noise, Interval x, Interval y) {
	// TODO More precise analysis
	const float amp = Math::abs(noise.get_amplitude());
	return math::Interval2{ //
		Interval{ x.min - amp, x.max + amp }, //
		Interval{ y.min - amp, y.max + amp }
	};
}

math::Interval3 get_fnl_gradient_range_3d(const ZN_FastNoiseLiteGradient &noise, Interval x, Interval y, Interval z) {
	// TODO More precise analysis
	const float amp = Math::abs(noise.get_amplitude());
	return math::Interval3{
		Interval{ x.min - amp, x.max + amp }, //
		Interval{ y.min - amp, y.max + amp }, //
		Interval{ z.min - amp, z.max + amp } //
	};
}

} // namespace zylann
