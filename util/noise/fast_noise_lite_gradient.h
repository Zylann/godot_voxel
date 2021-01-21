#ifndef FAST_NOISE_LITE_GRADIENT_H
#define FAST_NOISE_LITE_GRADIENT_H

#include <core/resource.h>

#include "../../thirdparty/fast_noise/FastNoiseLite.h"

// Domain warp is a transformation of coordinates before sampling the actual noise.
// It can be done with another instance of noise, however it needs a sample for each coordinate,
// so FastNoiseLite provides specialized versions of this using gradients.
// This is faster and produces higher-quality results.
//
// Note: FastNoiseLite provides this with the same class, but then its unclear which applies to what,
// so I made two classes, each with a specific purpose.
//
class FastNoiseLiteGradient : public Resource {
	GDCLASS(FastNoiseLiteGradient, Resource)

	typedef fast_noise_lite::FastNoiseLite _FastNoise;

public:
	// TODO Had to prefix it because of https://github.com/godotengine/godot/issues/44860
	static const int _MAX_OCTAVES = 32;

	enum NoiseType {
		TYPE_OPEN_SIMPLEX_2 = _FastNoise::DomainWarpType_OpenSimplex2,
		TYPE_OPEN_SIMPLEX_2_REDUCED = _FastNoise::DomainWarpType_OpenSimplex2Reduced,
		TYPE_VALUE = _FastNoise::DomainWarpType_BasicGrid
	};

	// This one does not map directly to FastNoise unfortunately,
	// because Godot's UI wants consecutive values starting from 0...
	enum FractalType {
		FRACTAL_NONE,
		FRACTAL_DOMAIN_WARP_PROGRESSIVE,
		FRACTAL_DOMAIN_WARP_INDEPENDENT
	};

	enum RotationType3D {
		ROTATION_3D_NONE = _FastNoise::RotationType3D_None,
		ROTATION_3D_IMPROVE_XY_PLANES = _FastNoise::RotationType3D_ImproveXYPlanes,
		ROTATION_3D_IMPROVE_XZ_PLANES = _FastNoise::RotationType3D_ImproveXZPlanes
	};

	FastNoiseLiteGradient();

	void set_noise_type(NoiseType type);
	NoiseType get_noise_type() const;

	void set_seed(int seed);
	int get_seed() const;

	void set_period(float p);
	float get_period() const;

	void set_amplitude(float amp);
	float get_amplitude() const;

	void set_fractal_type(FractalType type);
	FractalType get_fractal_type() const;

	void set_fractal_octaves(int octaves);
	int get_fractal_octaves() const;

	void set_fractal_lacunarity(float lacunarity);
	float get_fractal_lacunarity() const;

	void set_fractal_gain(float gain);
	float get_fractal_gain() const;

	void set_rotation_type_3d(RotationType3D type);
	RotationType3D get_rotation_type_3d() const;

	// These are inline to ensure inlining actually happens. If they were bound directly to the script API,
	// it means they would need to have an address, in which case I'm not sure they would be inlined?

	inline void warp_2d(float &x, float &y) const {
		return _fn.DomainWarp(x, y);
	}

	inline void warp_3d(float &x, float &y, float &z) const {
		return _fn.DomainWarp(x, y, z);
	}

	// TODO Bounds access
	// TODO Interval range analysis

private:
	static void _bind_methods();

	// TODO Getting the gradient instead of adding it would be more useful?

	Vector2 _b_warp_2d(Vector2 pos) {
		warp_2d(pos.x, pos.y);
		return pos;
	}

	Vector3 _b_warp_3d(Vector3 pos) {
		warp_3d(pos.x, pos.y, pos.z);
		return pos;
	}

	fast_noise_lite::FastNoiseLite _fn;

	// TODO FastNoiseLite should rather have getters

	NoiseType _noise_type = TYPE_VALUE;
	int _seed = 0;
	float _period = 64.f;
	float _amplitude = 30.f;

	FractalType _fractal_type = FRACTAL_NONE;
	int _fractal_octaves = 3;
	float _fractal_lacunarity = 2.f;
	float _fractal_gain = 0.5f;

	RotationType3D _rotation_type_3d = ROTATION_3D_NONE;
};

VARIANT_ENUM_CAST(FastNoiseLiteGradient::NoiseType);
VARIANT_ENUM_CAST(FastNoiseLiteGradient::FractalType);
VARIANT_ENUM_CAST(FastNoiseLiteGradient::RotationType3D);

#endif // FAST_NOISE_LITE_GRADIENT_H
