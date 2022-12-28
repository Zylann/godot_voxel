#include "fast_noise_lite.h"
#include "../../godot/core/array.h"
#include "../../godot/core/callable.h"
#include "../../math/funcs.h"
#include "../../string_funcs.h"

namespace zylann {

ZN_FastNoiseLite::ZN_FastNoiseLite() {
	_fn.SetNoiseType(static_cast<_FastNoise::NoiseType>(_noise_type));
	_fn.SetSeed(_seed);
	_fn.SetFrequency(1.f / _period);

	_fn.SetFractalType(static_cast<_FastNoise::FractalType>(_fractal_type));
	_fn.SetFractalOctaves(_fractal_octaves);
	_fn.SetFractalLacunarity(_fractal_lacunarity);
	_fn.SetFractalPingPongStrength(_fractal_ping_pong_strength);
	_fn.SetFractalGain(_fractal_gain);
	_fn.SetFractalWeightedStrength(_fractal_weighted_strength);

	_fn.SetCellularDistanceFunction(static_cast<_FastNoise::CellularDistanceFunction>(_cellular_distance_function));
	_fn.SetCellularReturnType(static_cast<_FastNoise::CellularReturnType>(_cellular_return_type));
	_fn.SetCellularJitter(_cellular_jitter);

	_fn.SetRotationType3D(static_cast<_FastNoise::RotationType3D>(_rotation_type_3d));
}

void ZN_FastNoiseLite::set_noise_type(NoiseType type) {
	if (_noise_type == type) {
		return;
	}
	_noise_type = type;
	_fn.SetNoiseType(static_cast<_FastNoise::NoiseType>(_noise_type));
	emit_changed();
}

ZN_FastNoiseLite::NoiseType ZN_FastNoiseLite::get_noise_type() const {
	return _noise_type;
}

void ZN_FastNoiseLite::set_seed(int seed) {
	if (_seed == seed) {
		return;
	}
	_seed = seed;
	_fn.SetSeed(seed);
	emit_changed();
}

int ZN_FastNoiseLite::get_seed() const {
	return _seed;
}

void ZN_FastNoiseLite::set_period(float p) {
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

float ZN_FastNoiseLite::get_period() const {
	return _period;
}

void ZN_FastNoiseLite::set_warp_noise(Ref<ZN_FastNoiseLiteGradient> warp_noise) {
	if (_warp_noise == warp_noise) {
		return;
	}

	if (_warp_noise.is_valid()) {
		_warp_noise->disconnect("changed", ZN_GODOT_CALLABLE_MP(this, ZN_FastNoiseLite, _on_warp_noise_changed));
	}

	_warp_noise = warp_noise;

	if (_warp_noise.is_valid()) {
		_warp_noise->connect("changed", ZN_GODOT_CALLABLE_MP(this, ZN_FastNoiseLite, _on_warp_noise_changed));
	}

	emit_changed();
}

Ref<ZN_FastNoiseLiteGradient> ZN_FastNoiseLite::get_warp_noise() const {
	return _warp_noise;
}

void ZN_FastNoiseLite::set_fractal_type(FractalType type) {
	if (_fractal_type == type) {
		return;
	}
	_fractal_type = type;
	_fn.SetFractalType(static_cast<_FastNoise::FractalType>(_fractal_type));
	emit_changed();
}

ZN_FastNoiseLite::FractalType ZN_FastNoiseLite::get_fractal_type() const {
	return _fractal_type;
}

void ZN_FastNoiseLite::set_fractal_octaves(int octaves) {
	ERR_FAIL_COND(octaves <= 0);
	if (octaves > MAX_OCTAVES) {
		octaves = MAX_OCTAVES;
	}
	if (_fractal_octaves == octaves) {
		return;
	}
	_fractal_octaves = octaves;
	_fn.SetFractalOctaves(octaves);
	emit_changed();
}

int ZN_FastNoiseLite::get_fractal_octaves() const {
	return _fractal_octaves;
}

void ZN_FastNoiseLite::set_fractal_lacunarity(float lacunarity) {
	if (_fractal_lacunarity == lacunarity) {
		return;
	}
	_fractal_lacunarity = lacunarity;
	_fn.SetFractalLacunarity(lacunarity);
	emit_changed();
}

float ZN_FastNoiseLite::get_fractal_lacunarity() const {
	return _fractal_lacunarity;
}

void ZN_FastNoiseLite::set_fractal_gain(float gain) {
	if (_fractal_gain == gain) {
		return;
	}
	_fractal_gain = gain;
	_fn.SetFractalGain(gain);
	emit_changed();
}

float ZN_FastNoiseLite::get_fractal_gain() const {
	return _fractal_gain;
}

void ZN_FastNoiseLite::set_fractal_ping_pong_strength(float s) {
	if (_fractal_ping_pong_strength == s) {
		return;
	}
	_fractal_ping_pong_strength = s;
	_fn.SetFractalPingPongStrength(s);
	emit_changed();
}

float ZN_FastNoiseLite::get_fractal_ping_pong_strength() const {
	return _fractal_ping_pong_strength;
}

void ZN_FastNoiseLite::set_fractal_weighted_strength(float s) {
	if (_fractal_weighted_strength == s) {
		return;
	}
	_fractal_weighted_strength = s;
	_fn.SetFractalWeightedStrength(s);
	emit_changed();
}

float ZN_FastNoiseLite::get_fractal_weighted_strength() const {
	return _fractal_weighted_strength;
}

void ZN_FastNoiseLite::set_cellular_distance_function(CellularDistanceFunction cdf) {
	if (cdf == _cellular_distance_function) {
		return;
	}
	_cellular_distance_function = cdf;
	_fn.SetCellularDistanceFunction(static_cast<_FastNoise::CellularDistanceFunction>(_cellular_distance_function));
	emit_changed();
}

ZN_FastNoiseLite::CellularDistanceFunction ZN_FastNoiseLite::get_cellular_distance_function() const {
	return _cellular_distance_function;
}

void ZN_FastNoiseLite::set_cellular_return_type(CellularReturnType rt) {
	if (_cellular_return_type == rt) {
		return;
	}
	_cellular_return_type = rt;
	_fn.SetCellularReturnType(static_cast<_FastNoise::CellularReturnType>(_cellular_return_type));
	emit_changed();
}

ZN_FastNoiseLite::CellularReturnType ZN_FastNoiseLite::get_cellular_return_type() const {
	return _cellular_return_type;
}

void ZN_FastNoiseLite::set_cellular_jitter(float jitter) {
	jitter = math::clamp(jitter, 0.f, 1.f);
	if (_cellular_jitter == jitter) {
		return;
	}
	_cellular_jitter = jitter;
	_fn.SetCellularJitter(_cellular_jitter);
	emit_changed();
}

float ZN_FastNoiseLite::get_cellular_jitter() const {
	return _cellular_jitter;
}

void ZN_FastNoiseLite::set_rotation_type_3d(RotationType3D type) {
	if (_rotation_type_3d == type) {
		return;
	}
	_rotation_type_3d = type;
	_fn.SetRotationType3D(static_cast<_FastNoise::RotationType3D>(_rotation_type_3d));
	emit_changed();
}

ZN_FastNoiseLite::RotationType3D ZN_FastNoiseLite::get_rotation_type_3d() const {
	return _rotation_type_3d;
}

void ZN_FastNoiseLite::_on_warp_noise_changed() {
	emit_changed();
}

void ZN_FastNoiseLite::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_noise_type", "type"), &ZN_FastNoiseLite::set_noise_type);
	ClassDB::bind_method(D_METHOD("get_noise_type"), &ZN_FastNoiseLite::get_noise_type);

	ClassDB::bind_method(D_METHOD("set_seed", "seed"), &ZN_FastNoiseLite::set_seed);
	ClassDB::bind_method(D_METHOD("get_seed"), &ZN_FastNoiseLite::get_seed);

	ClassDB::bind_method(D_METHOD("set_period", "period"), &ZN_FastNoiseLite::set_period);
	ClassDB::bind_method(D_METHOD("get_period"), &ZN_FastNoiseLite::get_period);

	ClassDB::bind_method(D_METHOD("set_warp_noise", "gradient_noise"), &ZN_FastNoiseLite::set_warp_noise);
	ClassDB::bind_method(D_METHOD("get_warp_noise"), &ZN_FastNoiseLite::get_warp_noise);

	ClassDB::bind_method(D_METHOD("set_fractal_type", "type"), &ZN_FastNoiseLite::set_fractal_type);
	ClassDB::bind_method(D_METHOD("get_fractal_type"), &ZN_FastNoiseLite::get_fractal_type);

	ClassDB::bind_method(D_METHOD("set_fractal_octaves", "octaves"), &ZN_FastNoiseLite::set_fractal_octaves);
	ClassDB::bind_method(D_METHOD("get_fractal_octaves"), &ZN_FastNoiseLite::get_fractal_octaves);

	ClassDB::bind_method(D_METHOD("set_fractal_lacunarity", "lacunarity"), &ZN_FastNoiseLite::set_fractal_lacunarity);
	ClassDB::bind_method(D_METHOD("get_fractal_lacunarity"), &ZN_FastNoiseLite::get_fractal_lacunarity);

	ClassDB::bind_method(D_METHOD("set_fractal_gain", "gain"), &ZN_FastNoiseLite::set_fractal_gain);
	ClassDB::bind_method(D_METHOD("get_fractal_gain"), &ZN_FastNoiseLite::get_fractal_gain);

	ClassDB::bind_method(
			D_METHOD("set_fractal_ping_pong_strength", "strength"), &ZN_FastNoiseLite::set_fractal_ping_pong_strength);
	ClassDB::bind_method(D_METHOD("get_fractal_ping_pong_strength"), &ZN_FastNoiseLite::get_fractal_ping_pong_strength);

	ClassDB::bind_method(
			D_METHOD("set_fractal_weighted_strength", "strength"), &ZN_FastNoiseLite::set_fractal_weighted_strength);
	ClassDB::bind_method(D_METHOD("get_fractal_weighted_strength"), &ZN_FastNoiseLite::get_fractal_weighted_strength);

	ClassDB::bind_method(D_METHOD("set_cellular_distance_function", "cell_distance_func"),
			&ZN_FastNoiseLite::set_cellular_distance_function);
	ClassDB::bind_method(D_METHOD("get_cellular_distance_function"), &ZN_FastNoiseLite::get_cellular_distance_function);

	ClassDB::bind_method(
			D_METHOD("set_cellular_return_type", "return_type"), &ZN_FastNoiseLite::set_cellular_return_type);
	ClassDB::bind_method(D_METHOD("get_cellular_return_type"), &ZN_FastNoiseLite::get_cellular_return_type);

	ClassDB::bind_method(D_METHOD("set_cellular_jitter", "return_type"), &ZN_FastNoiseLite::set_cellular_jitter);
	ClassDB::bind_method(D_METHOD("get_cellular_jitter"), &ZN_FastNoiseLite::get_cellular_jitter);

	ClassDB::bind_method(D_METHOD("set_rotation_type_3d", "type"), &ZN_FastNoiseLite::set_rotation_type_3d);
	ClassDB::bind_method(D_METHOD("get_rotation_type_3d"), &ZN_FastNoiseLite::get_rotation_type_3d);

	ClassDB::bind_method(D_METHOD("get_noise_2d", "x", "y"), &ZN_FastNoiseLite::_b_get_noise_2d);
	ClassDB::bind_method(D_METHOD("get_noise_3d", "x", "y", "z"), &ZN_FastNoiseLite::_b_get_noise_3d);

	ClassDB::bind_method(D_METHOD("get_noise_2dv", "position"), &ZN_FastNoiseLite::_b_get_noise_2dv);
	ClassDB::bind_method(D_METHOD("get_noise_3dv", "position"), &ZN_FastNoiseLite::_b_get_noise_3dv);

#ifdef ZN_GODOT_EXTENSION
	ClassDB::bind_method(D_METHOD("_on_warp_noise_changed"), &ZN_FastNoiseLite::_on_warp_noise_changed);
#endif

	ADD_PROPERTY(PropertyInfo(Variant::INT, "noise_type", PROPERTY_HINT_ENUM,
						 "OpenSimplex2,OpenSimplex2S,Cellular,Perlin,ValueCubic,Value"),
			"set_noise_type", "get_noise_type");

	ADD_PROPERTY(PropertyInfo(Variant::INT, "seed"), "set_seed", "get_seed");

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "period", PROPERTY_HINT_RANGE, "0.0001,10000.0,0.1,exp"), "set_period",
			"get_period");

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "warp_noise", PROPERTY_HINT_RESOURCE_TYPE,
						 ZN_FastNoiseLiteGradient::get_class_static()),
			"set_warp_noise", "get_warp_noise");

	ADD_GROUP("Fractal", "");

	ADD_PROPERTY(PropertyInfo(Variant::INT, "fractal_type", PROPERTY_HINT_ENUM, "None,FBm,Ridged,PingPong"),
			"set_fractal_type", "get_fractal_type");

	ADD_PROPERTY(PropertyInfo(Variant::INT, "fractal_octaves", PROPERTY_HINT_RANGE,
						 String("1,{0},1").format(varray(MAX_OCTAVES))),
			"set_fractal_octaves", "get_fractal_octaves");

	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "fractal_lacunarity"), "set_fractal_lacunarity", "get_fractal_lacunarity");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fractal_gain", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"), "set_fractal_gain",
			"get_fractal_gain");

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fractal_ping_pong_strength"), "set_fractal_ping_pong_strength",
			"get_fractal_ping_pong_strength");

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fractal_weighted_strength"), "set_fractal_weighted_strength",
			"get_fractal_weighted_strength");

	ADD_GROUP("Cellular", "");

	ADD_PROPERTY(PropertyInfo(Variant::INT, "cellular_distance_function", PROPERTY_HINT_ENUM,
						 "DistanceEuclidean,DistanceEuclideanSq,Manhattan,Hybrid"),
			"set_cellular_distance_function", "get_cellular_distance_function");

	ADD_PROPERTY(PropertyInfo(Variant::INT, "cellular_return_type", PROPERTY_HINT_ENUM,
						 "CellValue,Distance,Distance2,Distance2Add,Distance2Sub,Distance2Mul,Distance2Div"),
			"set_cellular_return_type", "get_cellular_return_type");

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "cellular_jitter", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"),
			"set_cellular_jitter", "get_cellular_jitter");

	ADD_GROUP("Advanced", "");

	ADD_PROPERTY(
			PropertyInfo(Variant::INT, "rotation_type_3d", PROPERTY_HINT_ENUM, "None,ImproveXYPlanes,ImproveXZPlanes"),
			"set_rotation_type_3d", "get_rotation_type_3d");

	BIND_ENUM_CONSTANT(TYPE_OPEN_SIMPLEX_2);
	BIND_ENUM_CONSTANT(TYPE_OPEN_SIMPLEX_2S);
	BIND_ENUM_CONSTANT(TYPE_CELLULAR);
	BIND_ENUM_CONSTANT(TYPE_PERLIN);
	BIND_ENUM_CONSTANT(TYPE_VALUE_CUBIC);
	BIND_ENUM_CONSTANT(TYPE_VALUE);

	BIND_ENUM_CONSTANT(FRACTAL_NONE);
	BIND_ENUM_CONSTANT(FRACTAL_FBM);
	BIND_ENUM_CONSTANT(FRACTAL_RIDGED);
	BIND_ENUM_CONSTANT(FRACTAL_PING_PONG);

	BIND_ENUM_CONSTANT(ROTATION_3D_NONE);
	BIND_ENUM_CONSTANT(ROTATION_3D_IMPROVE_XY_PLANES);
	BIND_ENUM_CONSTANT(ROTATION_3D_IMPROVE_XZ_PLANES);

	BIND_ENUM_CONSTANT(CELLULAR_DISTANCE_EUCLIDEAN);
	BIND_ENUM_CONSTANT(CELLULAR_DISTANCE_EUCLIDEAN_SQ);
	BIND_ENUM_CONSTANT(CELLULAR_DISTANCE_MANHATTAN);
	BIND_ENUM_CONSTANT(CELLULAR_DISTANCE_HYBRID);

	BIND_ENUM_CONSTANT(CELLULAR_RETURN_CELL_VALUE);
	BIND_ENUM_CONSTANT(CELLULAR_RETURN_DISTANCE);
	BIND_ENUM_CONSTANT(CELLULAR_RETURN_DISTANCE_2);
	BIND_ENUM_CONSTANT(CELLULAR_RETURN_DISTANCE_2_ADD);
	BIND_ENUM_CONSTANT(CELLULAR_RETURN_DISTANCE_2_SUB);
	BIND_ENUM_CONSTANT(CELLULAR_RETURN_DISTANCE_2_MUL);
	BIND_ENUM_CONSTANT(CELLULAR_RETURN_DISTANCE_2_DIV);
}

} // namespace zylann
