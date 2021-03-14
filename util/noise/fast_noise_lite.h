#ifndef FAST_NOISE_LITE_H
#define FAST_NOISE_LITE_H

#include <core/resource.h>

#include "fast_noise_lite_gradient.h"

class FastNoiseLite : public Resource {
	GDCLASS(FastNoiseLite, Resource)

	typedef fast_noise_lite::FastNoiseLite _FastNoise;

public:
	// TODO Had to prefix it because of https://github.com/godotengine/godot/issues/44860
	static const int _MAX_OCTAVES = 32;

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

	FastNoiseLite();

	void set_noise_type(NoiseType type);
	NoiseType get_noise_type() const;

	void set_seed(int seed);
	int get_seed() const;

	void set_period(float p);
	float get_period() const;

	void set_warp_noise(Ref<FastNoiseLiteGradient> warp_noise);
	Ref<FastNoiseLiteGradient> get_warp_noise() const;

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

	inline float get_noise_2d(float x, float y) const {
		if (_warp_noise.is_valid()) {
			_warp_noise->warp_2d(x, y);
		}
		return _fn.GetNoise(x, y);
	}

	inline float get_noise_3d(float x, float y, float z) const {
		if (_warp_noise.is_valid()) {
			_warp_noise->warp_3d(x, y, z);
		}
		return _fn.GetNoise(x, y, z);
	}

	// TODO Have a separate cell noise? It outputs multiple things, but we only get one.
	// To get the others the API forces to calculate it a second time, and it's the most expensive noise...

	// Internal

	const fast_noise_lite::FastNoiseLite &get_noise_internal() const {
		return _fn;
	}

private:
	static void _bind_methods();

	void _on_warp_noise_changed();

	float _b_get_noise_2d(float x, float y) { return get_noise_2d(x, y); }
	float _b_get_noise_3d(float x, float y, float z) { return get_noise_3d(x, y, z); }

	float _b_get_noise_2dv(Vector2 p) { return get_noise_2d(p.x, p.y); }
	float _b_get_noise_3dv(Vector3 p) { return get_noise_3d(p.x, p.y, p.z); }

	fast_noise_lite::FastNoiseLite _fn;

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

	Ref<FastNoiseLiteGradient> _warp_noise;
};

VARIANT_ENUM_CAST(FastNoiseLite::NoiseType);
VARIANT_ENUM_CAST(FastNoiseLite::FractalType);
VARIANT_ENUM_CAST(FastNoiseLite::RotationType3D);
VARIANT_ENUM_CAST(FastNoiseLite::CellularDistanceFunction);
VARIANT_ENUM_CAST(FastNoiseLite::CellularReturnType);

#endif // FAST_NOISE_LITE_H
