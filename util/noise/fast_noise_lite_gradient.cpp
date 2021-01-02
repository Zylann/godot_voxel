#include "fast_noise_lite_gradient.h"

static fast_noise_lite::FastNoiseLite::FractalType to_fnl_fractal_type(FastNoiseLiteGradient::FractalType type) {
	switch (type) {
		case FastNoiseLiteGradient::FRACTAL_NONE:
			return fast_noise_lite::FastNoiseLite::FractalType_None;

		case FastNoiseLiteGradient::FRACTAL_DOMAIN_WARP_PROGRESSIVE:
			return fast_noise_lite::FastNoiseLite::FractalType_DomainWarpProgressive;

		case FastNoiseLiteGradient::FRACTAL_DOMAIN_WARP_INDEPENDENT:
			return fast_noise_lite::FastNoiseLite::FractalType_DomainWarpIndependent;

		default:
			ERR_PRINT("Unknown type");
			break;
	}
	return fast_noise_lite::FastNoiseLite::FractalType_None;
}

FastNoiseLiteGradient::FastNoiseLiteGradient() {
	_fn.SetDomainWarpType(static_cast<_FastNoise::DomainWarpType>(_noise_type));
	_fn.SetSeed(_seed);
	_fn.SetFrequency(1.f / _period);
	_fn.SetDomainWarpAmp(_amplitude);

	_fn.SetFractalType(to_fnl_fractal_type(_fractal_type));
	_fn.SetFractalOctaves(_fractal_octaves);
	_fn.SetFractalLacunarity(_fractal_lacunarity);
	_fn.SetFractalGain(_fractal_gain);

	_fn.SetRotationType3D(static_cast<_FastNoise::RotationType3D>(_rotation_type_3d));
}

void FastNoiseLiteGradient::set_noise_type(NoiseType type) {
	if (_noise_type == type) {
		return;
	}
	_noise_type = type;
	_fn.SetDomainWarpType(static_cast<_FastNoise::DomainWarpType>(_noise_type));
	emit_changed();
}

FastNoiseLiteGradient::NoiseType FastNoiseLiteGradient::get_noise_type() const {
	return _noise_type;
}

void FastNoiseLiteGradient::set_seed(int seed) {
	if (_seed == seed) {
		return;
	}
	_seed = seed;
	_fn.SetSeed(seed);
	emit_changed();
}

int FastNoiseLiteGradient::get_seed() const {
	return _seed;
}

void FastNoiseLiteGradient::set_period(float p) {
	if (p < 0.0001f) {
		p = 0.0001f;
	}
	if (_period == p) {
		return;
	}
	_period = p;
	_fn.SetFrequency(1.f / _period);
	emit_changed();
}

float FastNoiseLiteGradient::get_period() const {
	return _period;
}

void FastNoiseLiteGradient::set_amplitude(float amp) {
	if (amp == _amplitude) {
		return;
	}
	_amplitude = amp;
	_fn.SetDomainWarpAmp(amp);
	emit_changed();
}

float FastNoiseLiteGradient::get_amplitude() const {
	return _amplitude;
}

void FastNoiseLiteGradient::set_fractal_type(FractalType type) {
	if (type == _fractal_type) {
		return;
	}
	_fractal_type = type;
	_fn.SetFractalType(to_fnl_fractal_type(_fractal_type));
	emit_changed();
}

FastNoiseLiteGradient::FractalType FastNoiseLiteGradient::get_fractal_type() const {
	return _fractal_type;
}

void FastNoiseLiteGradient::set_fractal_octaves(int octaves) {
	if (_fractal_octaves == octaves) {
		return;
	}
	_fractal_octaves = octaves;
	_fn.SetFractalOctaves(octaves);
	emit_changed();
}

int FastNoiseLiteGradient::get_fractal_octaves() const {
	return _fractal_octaves;
}

void FastNoiseLiteGradient::set_fractal_lacunarity(float lacunarity) {
	if (_fractal_lacunarity == lacunarity) {
		return;
	}
	_fractal_lacunarity = lacunarity;
	_fn.SetFractalLacunarity(lacunarity);
	emit_changed();
}

float FastNoiseLiteGradient::get_fractal_lacunarity() const {
	return _fractal_lacunarity;
}

void FastNoiseLiteGradient::set_fractal_gain(float gain) {
	if (_fractal_gain == gain) {
		return;
	}
	_fractal_gain = gain;
	_fn.SetFractalGain(gain);
	emit_changed();
}

float FastNoiseLiteGradient::get_fractal_gain() const {
	return _fractal_gain;
}

void FastNoiseLiteGradient::set_rotation_type_3d(RotationType3D type) {
	if (_rotation_type_3d == type) {
		return;
	}
	_rotation_type_3d = type;
	_fn.SetRotationType3D(static_cast<_FastNoise::RotationType3D>(_rotation_type_3d));
	emit_changed();
}

FastNoiseLiteGradient::RotationType3D FastNoiseLiteGradient::get_rotation_type_3d() const {
	return _rotation_type_3d;
}

void FastNoiseLiteGradient::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_noise_type", "type"), &FastNoiseLiteGradient::set_noise_type);
	ClassDB::bind_method(D_METHOD("get_noise_type"), &FastNoiseLiteGradient::get_noise_type);

	ClassDB::bind_method(D_METHOD("set_seed", "seed"), &FastNoiseLiteGradient::set_seed);
	ClassDB::bind_method(D_METHOD("get_seed"), &FastNoiseLiteGradient::get_seed);

	ClassDB::bind_method(D_METHOD("set_period", "period"), &FastNoiseLiteGradient::set_period);
	ClassDB::bind_method(D_METHOD("get_period"), &FastNoiseLiteGradient::get_period);

	ClassDB::bind_method(D_METHOD("set_amplitude", "amplitude"), &FastNoiseLiteGradient::set_amplitude);
	ClassDB::bind_method(D_METHOD("get_amplitude"), &FastNoiseLiteGradient::get_amplitude);

	ClassDB::bind_method(D_METHOD("set_fractal_type", "type"), &FastNoiseLiteGradient::set_fractal_type);
	ClassDB::bind_method(D_METHOD("get_fractal_type"), &FastNoiseLiteGradient::get_fractal_type);

	ClassDB::bind_method(D_METHOD("set_fractal_octaves", "octaves"), &FastNoiseLiteGradient::set_fractal_octaves);
	ClassDB::bind_method(D_METHOD("get_fractal_octaves"), &FastNoiseLiteGradient::get_fractal_octaves);

	ClassDB::bind_method(D_METHOD("set_fractal_lacunarity", "lacunarity"),
			&FastNoiseLiteGradient::set_fractal_lacunarity);
	ClassDB::bind_method(D_METHOD("get_fractal_lacunarity"), &FastNoiseLiteGradient::get_fractal_lacunarity);

	ClassDB::bind_method(D_METHOD("set_fractal_gain", "gain"), &FastNoiseLiteGradient::set_fractal_gain);
	ClassDB::bind_method(D_METHOD("get_fractal_gain"), &FastNoiseLiteGradient::get_fractal_gain);

	ClassDB::bind_method(D_METHOD("set_rotation_type_3d", "type"), &FastNoiseLiteGradient::set_rotation_type_3d);
	ClassDB::bind_method(D_METHOD("get_rotation_type_3d"), &FastNoiseLiteGradient::get_rotation_type_3d);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "noise_type", PROPERTY_HINT_ENUM,
						 "OpenSimplex2,OpenSimplex2Reduced,Value"),
			"set_noise_type", "get_noise_type");

	ADD_PROPERTY(PropertyInfo(Variant::INT, "seed"), "set_seed", "get_seed");

	ADD_PROPERTY(PropertyInfo(Variant::REAL, "period", PROPERTY_HINT_EXP_RANGE, "0.0001,10000.0"),
			"set_period", "get_period");

	ADD_PROPERTY(PropertyInfo(Variant::REAL, "amplitude", PROPERTY_HINT_EXP_RANGE, "0.0001,10000.0"),
			"set_amplitude", "get_amplitude");

	ADD_GROUP("Fractal", "");

	ADD_PROPERTY(PropertyInfo(Variant::INT, "fractal_type", PROPERTY_HINT_ENUM,
						 "None,DomainWarpProgressive,DomainWarpIndependent"),
			"set_fractal_type", "get_fractal_type");

	ADD_PROPERTY(PropertyInfo(Variant::INT, "fractal_octaves", PROPERTY_HINT_RANGE, vformat("1,%d,1", _MAX_OCTAVES)),
			"set_fractal_octaves", "get_fractal_octaves");

	ADD_PROPERTY(PropertyInfo(Variant::REAL, "fractal_lacunarity"), "set_fractal_lacunarity", "get_fractal_lacunarity");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "fractal_gain"), "set_fractal_gain", "get_fractal_gain");

	ADD_GROUP("Advanced", "");

	ADD_PROPERTY(PropertyInfo(Variant::INT, "rotation_type_3d", PROPERTY_HINT_ENUM,
						 "None,ImproveXYPlanes,ImproveXZPlanes"),
			"set_rotation_type_3d", "get_rotation_type_3d");

	BIND_ENUM_CONSTANT(TYPE_OPEN_SIMPLEX_2);
	BIND_ENUM_CONSTANT(TYPE_OPEN_SIMPLEX_2_REDUCED);
	BIND_ENUM_CONSTANT(TYPE_VALUE);

	BIND_ENUM_CONSTANT(FRACTAL_NONE);
	BIND_ENUM_CONSTANT(FRACTAL_DOMAIN_WARP_PROGRESSIVE);
	BIND_ENUM_CONSTANT(FRACTAL_DOMAIN_WARP_INDEPENDENT);

	BIND_ENUM_CONSTANT(ROTATION_3D_NONE);
	BIND_ENUM_CONSTANT(ROTATION_3D_IMPROVE_XY_PLANES);
	BIND_ENUM_CONSTANT(ROTATION_3D_IMPROVE_XZ_PLANES);
}
