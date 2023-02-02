#ifndef ZYLANN_FAST_NOISE_LITE_H
#define ZYLANN_FAST_NOISE_LITE_H

#include "fast_noise_lite_gradient.h"

namespace zylann {

// My own implementation of FastNoiseLite for Godot Engine.
// Godot 4 comes with its own FastNoiseLite, but mine predated it. So it needs to be prefixed to avoid conflict.
// Each has pros and cons. Godot's implementation has a few practical differences:
//
// - get_noise* methods are not inline. That means there is a potential performance loss when calling it many times
//  (basically all the time in this module).
//
// - get_noise* methods are not `const`. Means any person creating a Noise implementation can mutate internal state,
//   which is bad for multithreaded usage. IMO noise should not have state, and if it does, it must be explicit and not
//   change "lazily". Devs aren't sure yet if they should change that.
//
// - `real_t` is used everywhere, instead of just coordinates. That means builds with `float=64` might be slower,
//   especially in cases where such precision isn't necessary *for the use case of noise generation*.
//
// - Using it from a GDExtension has a lot more indirection and so will be much slower than a local implementation that
//   can benefit from inlining. That could be solved with `get_noise_series(Vector3 *positions, float *noises)`, but no
//   conventional Godot API allow to implement such a pattern without performance impact.
//
// - Domain warp is not exposed as its own thing, so can't generate from (x,y,z) coordinates in a single call
//
// - The internal instance of the FastNoiseLite object is not accessible, and it doesn't have the access changes present
//   in the module's version, so it is not possible to do range analysis more precisely. This is important for
//   `VoxelGeneratorGraph`.
//
// - Does not use GDVirtual, so it can only be extended by modules, and cannot be extended with GDExtensions
//
class ZN_FastNoiseLite : public Resource {
	GDCLASS(ZN_FastNoiseLite, Resource)

	typedef ::fast_noise_lite::FastNoiseLite _FastNoise;

public:
	static const int MAX_OCTAVES = 32;

	enum NoiseType {
		TYPE_OPEN_SIMPLEX_2 = _FastNoise::NoiseType_OpenSimplex2,
		TYPE_OPEN_SIMPLEX_2S = _FastNoise::NoiseType_OpenSimplex2S,
		TYPE_CELLULAR = _FastNoise::NoiseType_Cellular,
		TYPE_PERLIN = _FastNoise::NoiseType_Perlin,
		TYPE_VALUE_CUBIC = _FastNoise::NoiseType_ValueCubic,
		TYPE_VALUE = _FastNoise::NoiseType_Value
	};

	enum FractalType {
		FRACTAL_NONE = _FastNoise::FractalType_None,
		FRACTAL_FBM = _FastNoise::FractalType_FBm,
		FRACTAL_RIDGED = _FastNoise::FractalType_Ridged,
		FRACTAL_PING_PONG = _FastNoise::FractalType_PingPong
	};

	enum RotationType3D {
		ROTATION_3D_NONE = _FastNoise::RotationType3D_None,
		ROTATION_3D_IMPROVE_XY_PLANES = _FastNoise::RotationType3D_ImproveXYPlanes,
		ROTATION_3D_IMPROVE_XZ_PLANES = _FastNoise::RotationType3D_ImproveXZPlanes
	};

	enum CellularDistanceFunction {
		CELLULAR_DISTANCE_EUCLIDEAN = _FastNoise::CellularDistanceFunction_Euclidean,
		CELLULAR_DISTANCE_EUCLIDEAN_SQ = _FastNoise::CellularDistanceFunction_EuclideanSq,
		CELLULAR_DISTANCE_MANHATTAN = _FastNoise::CellularDistanceFunction_Manhattan,
		CELLULAR_DISTANCE_HYBRID = _FastNoise::CellularDistanceFunction_Hybrid
	};

	enum CellularReturnType {
		CELLULAR_RETURN_CELL_VALUE = _FastNoise::CellularReturnType_CellValue,
		CELLULAR_RETURN_DISTANCE = _FastNoise::CellularReturnType_Distance,
		CELLULAR_RETURN_DISTANCE_2 = _FastNoise::CellularReturnType_Distance2,
		CELLULAR_RETURN_DISTANCE_2_ADD = _FastNoise::CellularReturnType_Distance2Add,
		CELLULAR_RETURN_DISTANCE_2_SUB = _FastNoise::CellularReturnType_Distance2Sub,
		CELLULAR_RETURN_DISTANCE_2_MUL = _FastNoise::CellularReturnType_Distance2Mul,
		CELLULAR_RETURN_DISTANCE_2_DIV = _FastNoise::CellularReturnType_Distance2Div
	};

	ZN_FastNoiseLite();

	// Properties

	void set_noise_type(NoiseType type);
	NoiseType get_noise_type() const;

	void set_seed(int seed);
	int get_seed() const;

	void set_period(float p);
	float get_period() const;

	void set_warp_noise(Ref<ZN_FastNoiseLiteGradient> warp_noise);
	Ref<ZN_FastNoiseLiteGradient> get_warp_noise() const;

	void set_fractal_type(FractalType type);
	FractalType get_fractal_type() const;

	void set_fractal_octaves(int octaves);
	int get_fractal_octaves() const;

	void set_fractal_lacunarity(float lacunarity);
	float get_fractal_lacunarity() const;

	void set_fractal_gain(float gain);
	float get_fractal_gain() const;

	void set_fractal_ping_pong_strength(float s);
	float get_fractal_ping_pong_strength() const;

	void set_fractal_weighted_strength(float s);
	float get_fractal_weighted_strength() const;

	void set_cellular_distance_function(CellularDistanceFunction cdf);
	CellularDistanceFunction get_cellular_distance_function() const;

	void set_cellular_return_type(CellularReturnType rt);
	CellularReturnType get_cellular_return_type() const;

	void set_cellular_jitter(float jitter);
	float get_cellular_jitter() const;

	void set_rotation_type_3d(RotationType3D type);
	RotationType3D get_rotation_type_3d() const;

	// Queries

	inline float get_noise_2d(real_t x, real_t y) const {
		if (_warp_noise.is_valid()) {
			_warp_noise->warp_2d(x, y);
		}
		return _fn.GetNoise(x, y);
	}

	inline float get_noise_3d(real_t x, real_t y, real_t z) const {
		if (_warp_noise.is_valid()) {
			_warp_noise->warp_3d(x, y, z);
		}
		return _fn.GetNoise(x, y, z);
	}

	// TODO Have a separate cell noise? It outputs multiple things, but we only get one.
	// To get the others the API forces to calculate it a second time, and it's the most expensive noise...

	// Internal

	const ::fast_noise_lite::FastNoiseLite &get_noise_internal() const {
		return _fn;
	}

private:
	static void _bind_methods();

	void _on_warp_noise_changed();

	float _b_get_noise_2d(real_t x, real_t y) {
		return get_noise_2d(x, y);
	}
	float _b_get_noise_3d(real_t x, real_t y, real_t z) {
		return get_noise_3d(x, y, z);
	}

	float _b_get_noise_2dv(Vector2 p) {
		return get_noise_2d(p.x, p.y);
	}
	float _b_get_noise_3dv(Vector3 p) {
		return get_noise_3d(p.x, p.y, p.z);
	}

	::fast_noise_lite::FastNoiseLite _fn;

	// TODO FastNoiseLite should rather have getters

	NoiseType _noise_type = TYPE_OPEN_SIMPLEX_2;
	int _seed = 0;
	float _period = 64.f;

	FractalType _fractal_type = FRACTAL_FBM;
	int _fractal_octaves = 3;
	float _fractal_lacunarity = 2.f;
	float _fractal_ping_pong_strength = 2.f;
	float _fractal_gain = 0.5f;
	float _fractal_weighted_strength = 0.f;

	CellularDistanceFunction _cellular_distance_function = CELLULAR_DISTANCE_EUCLIDEAN_SQ;
	CellularReturnType _cellular_return_type = CELLULAR_RETURN_DISTANCE;
	float _cellular_jitter = 1.f;

	RotationType3D _rotation_type_3d = ROTATION_3D_NONE;

	Ref<ZN_FastNoiseLiteGradient> _warp_noise;
};

} // namespace zylann

VARIANT_ENUM_CAST(zylann::ZN_FastNoiseLite::NoiseType);
VARIANT_ENUM_CAST(zylann::ZN_FastNoiseLite::FractalType);
VARIANT_ENUM_CAST(zylann::ZN_FastNoiseLite::RotationType3D);
VARIANT_ENUM_CAST(zylann::ZN_FastNoiseLite::CellularDistanceFunction);
VARIANT_ENUM_CAST(zylann::ZN_FastNoiseLite::CellularReturnType);

#endif // ZYLANN_FAST_NOISE_LITE_H
