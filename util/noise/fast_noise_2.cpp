#include "fast_noise_2.h"
#include "../math/funcs.h"
#include "../math/vector3.h"
#include <core/io/image.h>

namespace zylann {

FastNoise2::FastNoise2() {
	// Setup default
	update_generator();
}

void FastNoise2::set_encoded_node_tree(String data) {
	if (data != _last_set_encoded_node_tree) {
		_last_set_encoded_node_tree = data;
		emit_changed();
	}
}

bool FastNoise2::is_valid() const {
	return _generator.get() != nullptr;
}

String FastNoise2::get_encoded_node_tree() const {
	// There is no way to get back an encoded node tree from `FastNoise::SmartNode<>`
	return _last_set_encoded_node_tree;
}

FastNoise2::SIMDLevel FastNoise2::get_simd_level() const {
	ERR_FAIL_COND_V(!is_valid(), SIMD_NULL);
	return SIMDLevel(_generator->GetSIMDLevel());
}

String FastNoise2::get_simd_level_name(SIMDLevel level) {
	switch (level) {
		case SIMD_NULL:
			return "Null";
		case SIMD_SCALAR:
			return "Scalar";
		case SIMD_SSE:
			return "SSE";
		case SIMD_SSE2:
			return "SSE2";
		case SIMD_SSE3:
			return "SSE3";
		case SIMD_SSE41:
			return "SSE41";
		case SIMD_SSE42:
			return "SSE42";
		case SIMD_AVX:
			return "AVX";
		case SIMD_AVX2:
			return "AVX2";
		case SIMD_AVX512:
			return "AVX512";
		case SIMD_NEON:
			return "NEON";
		default:
			ERR_PRINT(String("Unknown SIMD level {0}").format(varray(level)));
			return "Error";
	}
}

void FastNoise2::set_seed(int seed) {
	if (_seed == seed) {
		return;
	}
	_seed = seed;
	emit_changed();
}

int FastNoise2::get_seed() const {
	return _seed;
}

void FastNoise2::set_noise_type(NoiseType type) {
	if (_noise_type == type) {
		return;
	}
	_noise_type = type;
	emit_changed();
}

FastNoise2::NoiseType FastNoise2::get_noise_type() const {
	return _noise_type;
}

void FastNoise2::set_period(float p) {
	if (p < 0.0001f) {
		p = 0.0001f;
	}
	if (_period == p) {
		return;
	}
	_period = p;
	emit_changed();
}

float FastNoise2::get_period() const {
	return _period;
}

void FastNoise2::set_fractal_octaves(int octaves) {
	ERR_FAIL_COND(octaves <= 0);
	if (octaves > MAX_OCTAVES) {
		octaves = MAX_OCTAVES;
	}
	if (_fractal_octaves == octaves) {
		return;
	}
	_fractal_octaves = octaves;
	emit_changed();
}

void FastNoise2::set_fractal_type(FractalType type) {
	if (_fractal_type == type) {
		return;
	}
	_fractal_type = type;
	emit_changed();
}

FastNoise2::FractalType FastNoise2::get_fractal_type() const {
	return _fractal_type;
}

int FastNoise2::get_fractal_octaves() const {
	return _fractal_octaves;
}

void FastNoise2::set_fractal_lacunarity(float lacunarity) {
	if (_fractal_lacunarity == lacunarity) {
		return;
	}
	_fractal_lacunarity = lacunarity;
	emit_changed();
}

float FastNoise2::get_fractal_lacunarity() const {
	return _fractal_lacunarity;
}

void FastNoise2::set_fractal_gain(float gain) {
	if (_fractal_gain == gain) {
		return;
	}
	_fractal_gain = gain;
	emit_changed();
}

float FastNoise2::get_fractal_gain() const {
	return _fractal_gain;
}

void FastNoise2::set_fractal_ping_pong_strength(float s) {
	if (_fractal_ping_pong_strength == s) {
		return;
	}
	_fractal_ping_pong_strength = s;
	emit_changed();
}

float FastNoise2::get_fractal_ping_pong_strength() const {
	return _fractal_ping_pong_strength;
}

void FastNoise2::set_terrace_enabled(bool enable) {
	if (enable == _terrace_enabled) {
		return;
	}
	_terrace_enabled = enable;
	emit_changed();
}

bool FastNoise2::is_terrace_enabled() const {
	return _terrace_enabled;
}

void FastNoise2::set_terrace_multiplier(float m) {
	const float clamped_multiplier = math::max(m, 0.f);
	if (clamped_multiplier == _terrace_multiplier) {
		return;
	}
	_terrace_multiplier = clamped_multiplier;
	emit_changed();
}

float FastNoise2::get_terrace_multiplier() const {
	return _terrace_multiplier;
}

void FastNoise2::set_terrace_smoothness(float s) {
	const float clamped_smoothness = math::max(s, 0.f);
	if (_terrace_smoothness == clamped_smoothness) {
		return;
	}
	_terrace_smoothness = clamped_smoothness;
	emit_changed();
}

float FastNoise2::get_terrace_smoothness() const {
	return _terrace_smoothness;
}

void FastNoise2::set_remap_enabled(bool enabled) {
	if (enabled != _remap_enabled) {
		_remap_enabled = enabled;
		emit_changed();
	}
}

bool FastNoise2::is_remap_enabled() const {
	return _remap_enabled;
}

void FastNoise2::set_remap_input_min(float min_value) {
	if (min_value != _remap_src_min) {
		_remap_src_min = min_value;
		emit_changed();
	}
}

float FastNoise2::get_remap_input_min() const {
	return _remap_src_min;
}

void FastNoise2::set_remap_input_max(float max_value) {
	if (max_value != _remap_src_max) {
		_remap_src_max = max_value;
		emit_changed();
	}
}

float FastNoise2::get_remap_input_max() const {
	return _remap_src_max;
}

void FastNoise2::set_remap_output_min(float min_value) {
	if (min_value != _remap_dst_min) {
		_remap_dst_min = min_value;
		emit_changed();
	}
}

float FastNoise2::get_remap_output_min() const {
	return _remap_dst_min;
}

void FastNoise2::set_remap_output_max(float max_value) {
	if (max_value != _remap_dst_max) {
		_remap_dst_max = max_value;
		emit_changed();
	}
}

float FastNoise2::get_remap_output_max() const {
	return _remap_dst_max;
}

void FastNoise2::set_cellular_distance_function(CellularDistanceFunction cdf) {
	if (cdf == _cellular_distance_function) {
		return;
	}
	_cellular_distance_function = cdf;
	emit_changed();
}

FastNoise2::CellularDistanceFunction FastNoise2::get_cellular_distance_function() const {
	return _cellular_distance_function;
}

void FastNoise2::set_cellular_return_type(CellularReturnType rt) {
	if (_cellular_return_type == rt) {
		return;
	}
	_cellular_return_type = rt;
	emit_changed();
}

FastNoise2::CellularReturnType FastNoise2::get_cellular_return_type() const {
	return _cellular_return_type;
}

void FastNoise2::set_cellular_jitter(float jitter) {
	jitter = math::clamp(jitter, 0.f, 1.f);
	if (_cellular_jitter == jitter) {
		return;
	}
	_cellular_jitter = jitter;
	emit_changed();
}

float FastNoise2::get_cellular_jitter() const {
	return _cellular_jitter;
}

float FastNoise2::get_noise_2d_single(Vector2 pos) const {
	ERR_FAIL_COND_V(!is_valid(), 0.0);
	return _generator->GenSingle2D(pos.x, pos.y, _seed);
}

float FastNoise2::get_noise_3d_single(Vector3 pos) const {
	ERR_FAIL_COND_V(!is_valid(), 0.0);
	return _generator->GenSingle3D(pos.x, pos.y, pos.z, _seed);
}

void FastNoise2::get_noise_2d_series(Span<const float> src_x, Span<const float> src_y, Span<float> dst) const {
	ERR_FAIL_COND(!is_valid());
	ERR_FAIL_COND(src_x.size() != src_y.size() || src_x.size() != dst.size());
	if (src_x.size() < MIN_BUFFER_SIZE) {
		// Using backing arrays for input buffers, because SIMD needs to read multiple values. This might make
		// single-reads a bit slower, but in that case performance likely doesn't matter anyways
		FixedArray<float, MIN_BUFFER_SIZE> x;
		FixedArray<float, MIN_BUFFER_SIZE> y;
		FixedArray<float, MIN_BUFFER_SIZE> n;
		for (unsigned int i = 0; i < src_x.size(); ++i) {
			x[i] = src_x[i];
		}
		for (unsigned int i = 0; i < src_y.size(); ++i) {
			y[i] = src_y[i];
		}
		src_x = to_span(x);
		src_y = to_span(y);
		// TODO Need to update FastNoise2.
		// Using a destination buffer smaller than SIMD level is not supposed to break, but it crashes. Using a backing
		// array too as workaround.
		_generator->GenPositionArray2D(n.data(), n.size(), src_x.data(), src_y.data(), 0, 0, _seed);
		for (unsigned int i = 0; i < dst.size(); ++i) {
			dst[i] = n[i];
		}
	} else {
		_generator->GenPositionArray2D(dst.data(), dst.size(), src_x.data(), src_y.data(), 0, 0, _seed);
	}
}

void FastNoise2::get_noise_3d_series(
		Span<const float> src_x, Span<const float> src_y, Span<const float> src_z, Span<float> dst) const {
	ERR_FAIL_COND(!is_valid());
	ERR_FAIL_COND(src_x.size() != src_y.size() || src_x.size() != src_z.size() || src_x.size() != dst.size());
	if (src_x.size() < MIN_BUFFER_SIZE) {
		FixedArray<float, MIN_BUFFER_SIZE> x;
		FixedArray<float, MIN_BUFFER_SIZE> y;
		FixedArray<float, MIN_BUFFER_SIZE> z;
		FixedArray<float, MIN_BUFFER_SIZE> n;
		for (unsigned int i = 0; i < src_x.size(); ++i) {
			x[i] = src_x[i];
		}
		for (unsigned int i = 0; i < src_y.size(); ++i) {
			y[i] = src_y[i];
		}
		for (unsigned int i = 0; i < src_z.size(); ++i) {
			z[i] = src_z[i];
		}
		src_x = to_span(x);
		src_y = to_span(y);
		src_z = to_span(z);
		// TODO Need to update FastNoise2.
		// Using a destination buffer smaller than SIMD level is not supposed to break, but it crashes. Using a backing
		// array too as workaround.
		_generator->GenPositionArray3D(n.data(), n.size(), src_x.data(), src_y.data(), src_z.data(), 0, 0, 0, _seed);
		for (unsigned int i = 0; i < dst.size(); ++i) {
			dst[i] = n[i];
		}
	} else {
		_generator->GenPositionArray3D(
				dst.data(), dst.size(), src_x.data(), src_y.data(), src_z.data(), 0, 0, 0, _seed);
	}
}

void FastNoise2::get_noise_2d_grid(Vector2 origin, Vector2i size, Span<float> dst) const {
	ERR_FAIL_COND(!is_valid());
	ERR_FAIL_COND(size.x < 0 || size.y < 0);
	ERR_FAIL_COND(dst.size() != size_t(size.x) * size_t(size.y));
	_generator->GenUniformGrid2D(dst.data(), origin.x, origin.y, size.x, size.y, 1.f, _seed);
}

void FastNoise2::get_noise_3d_grid(Vector3 origin, Vector3i size, Span<float> dst) const {
	ERR_FAIL_COND(!is_valid());
	ERR_FAIL_COND(!math::is_valid_size(size));
	ERR_FAIL_COND(dst.size() != size_t(size.x) * size_t(size.y) * size_t(size.z));
	_generator->GenUniformGrid3D(dst.data(), origin.x, origin.y, origin.z, size.x, size.y, size.z, 1.f, _seed);
}

void FastNoise2::get_noise_2d_grid_tileable(Vector2i size, Span<float> dst) const {
	ERR_FAIL_COND(!is_valid());
	ERR_FAIL_COND(size.x < 0 || size.y < 0);
	ERR_FAIL_COND(dst.size() != size_t(size.x) * size_t(size.y));
	_generator->GenTileable2D(dst.data(), size.x, size.y, 1.f, _seed);
}

void FastNoise2::generate_image(Ref<Image> image, bool tileable) const {
	ERR_FAIL_COND(!is_valid());
	ERR_FAIL_COND(image.is_null());

	std::vector<float> buffer;
	buffer.resize(image->get_width() * image->get_height());

	if (tileable) {
		get_noise_2d_grid_tileable(Vector2i(image->get_width(), image->get_height()), to_span(buffer));
	} else {
		get_noise_2d_grid(Vector2(), Vector2i(image->get_width(), image->get_height()), to_span(buffer));
	}

	unsigned int i = 0;
	for (int y = 0; y < image->get_height(); ++y) {
		for (int x = 0; x < image->get_width(); ++x) {
#ifdef DEBUG_ENABLED
			CRASH_COND(i >= buffer.size());
#endif
			// Assuming -1..1 output. Some noise types can have different range though.
			const float n = buffer[i] * 0.5f + 0.5f;
			++i;
			image->set_pixel(x, y, Color(n, n, n));
		}
	}
}

void FastNoise2::update_generator() {
	if (_noise_type == TYPE_ENCODED_NODE_TREE) {
		CharString cs = _last_set_encoded_node_tree.utf8();
		// TODO FastNoise2 crashes if given an empty string.
		ERR_FAIL_COND_MSG(cs.length() == 0, "Encoded node tree is empty.");
		_generator = FastNoise::NewFromEncodedNodeTree(cs.get_data());
		ERR_FAIL_COND_MSG(!is_valid(), "Encoded node tree is invalid.");
		// TODO Maybe apply period modifier here?
		// NoiseTool assumes we scale input coordinates so typical noise made in there has period 1...
		return;
	}

	FastNoise::SmartNode<FastNoise::Generator> noise_node;
	switch (_noise_type) {
		case TYPE_OPEN_SIMPLEX_2:
			noise_node = FastNoise::New<FastNoise::OpenSimplex2>();
			break;

		case TYPE_SIMPLEX:
			noise_node = FastNoise::New<FastNoise::Simplex>();
			break;

		case TYPE_PERLIN:
			noise_node = FastNoise::New<FastNoise::Perlin>();
			break;

		case TYPE_VALUE:
			noise_node = FastNoise::New<FastNoise::Value>();
			break;

		case TYPE_CELLULAR: {
			FastNoise::SmartNode<FastNoise::CellularDistance> cd = FastNoise::New<FastNoise::CellularDistance>();
			cd->SetDistanceFunction(FastNoise::DistanceFunction(_cellular_distance_function));
			cd->SetReturnType(FastNoise::CellularDistance::ReturnType(_cellular_return_type));
			cd->SetJitterModifier(_cellular_jitter);
			noise_node = cd;
		} break;

		default:
			ERR_PRINT(String("Unknown noise type {0}").format(varray(_noise_type)));
			return;
	}

	ERR_FAIL_COND(noise_node.get() == nullptr);
	FastNoise::SmartNode<> generator_node = noise_node;

	if (_period != 1.f) {
		FastNoise::SmartNode<FastNoise::DomainScale> scale_node = FastNoise::New<FastNoise::DomainScale>();
		scale_node->SetScale(1.f / _period);
		scale_node->SetSource(generator_node);
		generator_node = scale_node;
	}

	FastNoise::SmartNode<FastNoise::Fractal<>> fractal_node;
	switch (_fractal_type) {
		case FRACTAL_NONE:
			break;

		case FRACTAL_FBM:
			fractal_node = FastNoise::New<FastNoise::FractalFBm>();
			break;

		case FRACTAL_PING_PONG: {
			FastNoise::SmartNode<FastNoise::FractalPingPong> pp_node = FastNoise::New<FastNoise::FractalPingPong>();
			pp_node->SetPingPongStrength(_fractal_ping_pong_strength);
			fractal_node = pp_node;
		} break;

		case FRACTAL_RIDGED:
			fractal_node = FastNoise::New<FastNoise::FractalRidged>();
			break;

		default:
			ERR_PRINT(String("Unknown fractal type {0}").format(varray(_fractal_type)));
			return;
	}

	if (fractal_node) {
		fractal_node->SetGain(_fractal_gain);
		fractal_node->SetLacunarity(_fractal_lacunarity);
		fractal_node->SetOctaveCount(_fractal_octaves);
		//fractal_node->SetWeightedStrength(_fractal_weighted_strength);
		fractal_node->SetSource(generator_node);
		generator_node = fractal_node;
	}

	if (_terrace_enabled) {
		FastNoise::SmartNode<FastNoise::Terrace> terrace_node = FastNoise::New<FastNoise::Terrace>();
		terrace_node->SetMultiplier(_terrace_multiplier);
		terrace_node->SetSmoothness(_terrace_smoothness);
		terrace_node->SetSource(generator_node);
		generator_node = terrace_node;
	}

	if (_remap_enabled) {
		FastNoise::SmartNode<FastNoise::Remap> remap_node = FastNoise::New<FastNoise::Remap>();
		remap_node->SetRemap(_remap_src_min, _remap_src_max, _remap_dst_min, _remap_dst_max);
		remap_node->SetSource(generator_node);
		generator_node = remap_node;
	}

	ERR_FAIL_COND(generator_node.get() == nullptr);
	_generator = generator_node;
}

math::Interval FastNoise2::get_estimated_output_range() const {
	// TODO Optimize: better range analysis on FastNoise2
	// Most noises should have known bounds like FastNoiseLite, but the node-graph nature of this library
	// can make it difficult to calculate. Would be nice if the library could provide that out of the box.
	if (is_remap_enabled()) {
		return math::Interval::from_unordered_values(get_remap_output_min(), get_remap_output_max());
	} else {
		return math::Interval(-1.f, 1.f);
	}
}

String FastNoise2::_b_get_simd_level_name(SIMDLevel level) {
	return get_simd_level_name(level);
}

void FastNoise2::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_noise_type", "type"), &FastNoise2::set_noise_type);
	ClassDB::bind_method(D_METHOD("get_noise_type"), &FastNoise2::get_noise_type);

	ClassDB::bind_method(D_METHOD("set_seed", "seed"), &FastNoise2::set_seed);
	ClassDB::bind_method(D_METHOD("get_seed"), &FastNoise2::get_seed);

	ClassDB::bind_method(D_METHOD("set_period", "period"), &FastNoise2::set_period);
	ClassDB::bind_method(D_METHOD("get_period"), &FastNoise2::get_period);

	// ClassDB::bind_method(D_METHOD("set_warp_noise", "gradient_noise"), &FastNoise2::set_warp_noise);
	// ClassDB::bind_method(D_METHOD("get_warp_noise"), &FastNoise2::get_warp_noise);

	ClassDB::bind_method(D_METHOD("set_fractal_type", "type"), &FastNoise2::set_fractal_type);
	ClassDB::bind_method(D_METHOD("get_fractal_type"), &FastNoise2::get_fractal_type);

	ClassDB::bind_method(D_METHOD("set_fractal_octaves", "octaves"), &FastNoise2::set_fractal_octaves);
	ClassDB::bind_method(D_METHOD("get_fractal_octaves"), &FastNoise2::get_fractal_octaves);

	ClassDB::bind_method(D_METHOD("set_fractal_lacunarity", "lacunarity"), &FastNoise2::set_fractal_lacunarity);
	ClassDB::bind_method(D_METHOD("get_fractal_lacunarity"), &FastNoise2::get_fractal_lacunarity);

	ClassDB::bind_method(D_METHOD("set_fractal_gain", "gain"), &FastNoise2::set_fractal_gain);
	ClassDB::bind_method(D_METHOD("get_fractal_gain"), &FastNoise2::get_fractal_gain);

	ClassDB::bind_method(
			D_METHOD("set_fractal_ping_pong_strength", "strength"), &FastNoise2::set_fractal_ping_pong_strength);
	ClassDB::bind_method(D_METHOD("get_fractal_ping_pong_strength"), &FastNoise2::get_fractal_ping_pong_strength);

	// ClassDB::bind_method(
	// 		D_METHOD("set_fractal_weighted_strength", "strength"), &FastNoise2::set_fractal_weighted_strength);
	// ClassDB::bind_method(D_METHOD("get_fractal_weighted_strength"), &FastNoise2::get_fractal_weighted_strength);

	ClassDB::bind_method(D_METHOD("set_cellular_distance_function", "cell_distance_func"),
			&FastNoise2::set_cellular_distance_function);
	ClassDB::bind_method(D_METHOD("get_cellular_distance_function"), &FastNoise2::get_cellular_distance_function);

	ClassDB::bind_method(D_METHOD("set_cellular_return_type", "return_type"), &FastNoise2::set_cellular_return_type);
	ClassDB::bind_method(D_METHOD("get_cellular_return_type"), &FastNoise2::get_cellular_return_type);

	ClassDB::bind_method(D_METHOD("set_cellular_jitter", "return_type"), &FastNoise2::set_cellular_jitter);
	ClassDB::bind_method(D_METHOD("get_cellular_jitter"), &FastNoise2::get_cellular_jitter);

	// ClassDB::bind_method(D_METHOD("set_rotation_type_3d", "type"), &FastNoiseLite::set_rotation_type_3d);
	// ClassDB::bind_method(D_METHOD("get_rotation_type_3d"), &FastNoiseLite::get_rotation_type_3d);

	ClassDB::bind_method(D_METHOD("set_terrace_enabled", "enabled"), &FastNoise2::set_terrace_enabled);
	ClassDB::bind_method(D_METHOD("is_terrace_enabled"), &FastNoise2::is_terrace_enabled);

	ClassDB::bind_method(D_METHOD("set_terrace_multiplier", "multiplier"), &FastNoise2::set_terrace_multiplier);
	ClassDB::bind_method(D_METHOD("get_terrace_multiplier"), &FastNoise2::get_terrace_multiplier);

	ClassDB::bind_method(D_METHOD("set_terrace_smoothness", "smoothness"), &FastNoise2::set_terrace_smoothness);
	ClassDB::bind_method(D_METHOD("get_terrace_smoothness"), &FastNoise2::get_terrace_smoothness);

	ClassDB::bind_method(D_METHOD("set_remap_enabled", "enabled"), &FastNoise2::set_remap_enabled);
	ClassDB::bind_method(D_METHOD("is_remap_enabled"), &FastNoise2::is_remap_enabled);

	ClassDB::bind_method(D_METHOD("set_remap_input_min", "min_value"), &FastNoise2::set_remap_input_min);
	ClassDB::bind_method(D_METHOD("get_remap_input_min"), &FastNoise2::get_remap_input_min);

	ClassDB::bind_method(D_METHOD("set_remap_input_max", "max_value"), &FastNoise2::set_remap_input_max);
	ClassDB::bind_method(D_METHOD("get_remap_input_max"), &FastNoise2::get_remap_input_max);

	ClassDB::bind_method(D_METHOD("set_remap_output_min", "min_value"), &FastNoise2::set_remap_output_min);
	ClassDB::bind_method(D_METHOD("get_remap_output_min"), &FastNoise2::get_remap_output_min);

	ClassDB::bind_method(D_METHOD("set_remap_output_max", "max_value"), &FastNoise2::set_remap_output_max);
	ClassDB::bind_method(D_METHOD("get_remap_output_max"), &FastNoise2::get_remap_output_max);

	ClassDB::bind_method(D_METHOD("set_encoded_node_tree", "code"), &FastNoise2::set_encoded_node_tree);
	ClassDB::bind_method(D_METHOD("get_encoded_node_tree"), &FastNoise2::get_encoded_node_tree);

	ClassDB::bind_method(D_METHOD("get_noise_2d_single", "pos"), &FastNoise2::get_noise_2d_single);
	ClassDB::bind_method(D_METHOD("get_noise_3d_single", "pos"), &FastNoise2::get_noise_3d_single);

	ClassDB::bind_method(D_METHOD("generate_image", "image", "tileable"), &FastNoise2::generate_image);

	ClassDB::bind_method(D_METHOD("get_simd_level_name", "level"), &FastNoise2::_b_get_simd_level_name);

	ClassDB::bind_method(D_METHOD("update_generator"), &FastNoise2::update_generator);

	// ClassDB::bind_method(D_METHOD("_on_warp_noise_changed"), &FastNoiseLite::_on_warp_noise_changed);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "noise_type", PROPERTY_HINT_ENUM, NOISE_TYPE_HINT_STRING), "set_noise_type",
			"get_noise_type");

	ADD_PROPERTY(PropertyInfo(Variant::INT, "seed"), "set_seed", "get_seed");

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "period", PROPERTY_HINT_RANGE, "0.0001,10000.0,0.1,exp"), "set_period",
			"get_period");

	// ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "warp_noise", PROPERTY_HINT_RESOURCE_TYPE, "FastNoiseLiteGradient"),
	// 		"set_warp_noise", "get_warp_noise");

	ADD_GROUP("Fractal", "");

	ADD_PROPERTY(PropertyInfo(Variant::INT, "fractal_type", PROPERTY_HINT_ENUM, FRACTAL_TYPE_HINT_STRING),
			"set_fractal_type", "get_fractal_type");

	ADD_PROPERTY(PropertyInfo(Variant::INT, "fractal_octaves", PROPERTY_HINT_RANGE, vformat("1,%d,1", MAX_OCTAVES)),
			"set_fractal_octaves", "get_fractal_octaves");

	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "fractal_lacunarity"), "set_fractal_lacunarity", "get_fractal_lacunarity");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fractal_gain"), "set_fractal_gain", "get_fractal_gain");

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fractal_ping_pong_strength"), "set_fractal_ping_pong_strength",
			"get_fractal_ping_pong_strength");

	// ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fractal_weighted_strength"), "set_fractal_weighted_strength",
	// 		"get_fractal_weighted_strength");

	ADD_GROUP("Cellular", "");

	ADD_PROPERTY(PropertyInfo(Variant::INT, "cellular_distance_function", PROPERTY_HINT_ENUM,
						 CELLULAR_DISTANCE_FUNCTION_HINT_STRING),
			"set_cellular_distance_function", "get_cellular_distance_function");

	ADD_PROPERTY(
			PropertyInfo(Variant::INT, "cellular_return_type", PROPERTY_HINT_ENUM, CELLULAR_RETURN_TYPE_HINT_STRING),
			"set_cellular_return_type", "get_cellular_return_type");

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "cellular_jitter", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"),
			"set_cellular_jitter", "get_cellular_jitter");

	ADD_GROUP("Terrace", "");

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "terrace_enabled"), "set_terrace_enabled", "is_terrace_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "terrace_multiplier", PROPERTY_HINT_RANGE, "0.0,100.0,0.1"),
			"set_terrace_multiplier", "get_terrace_multiplier");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "terrace_smoothness", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"),
			"set_terrace_smoothness", "get_terrace_smoothness");

	ADD_GROUP("Remap", "");

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "remap_enabled"), "set_remap_enabled", "is_remap_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "remap_input_min"), "set_remap_input_min", "get_remap_input_min");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "remap_input_max"), "set_remap_input_max", "get_remap_input_max");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "remap_output_min"), "set_remap_output_min", "get_remap_output_min");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "remap_output_max"), "set_remap_output_max", "get_remap_output_max");

	ADD_GROUP("Advanced", "");

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "encoded_node_tree"), "set_encoded_node_tree", "get_encoded_node_tree");

	// ADD_PROPERTY(
	// 		PropertyInfo(Variant::INT, "rotation_type_3d", PROPERTY_HINT_ENUM, "None,ImproveXYPlanes,ImproveXZPlanes"),
	// 		"set_rotation_type_3d", "get_rotation_type_3d");

	BIND_ENUM_CONSTANT(TYPE_OPEN_SIMPLEX_2);
	BIND_ENUM_CONSTANT(TYPE_SIMPLEX);
	BIND_ENUM_CONSTANT(TYPE_PERLIN);
	BIND_ENUM_CONSTANT(TYPE_VALUE);
	BIND_ENUM_CONSTANT(TYPE_CELLULAR);
	BIND_ENUM_CONSTANT(TYPE_ENCODED_NODE_TREE);

	BIND_ENUM_CONSTANT(FRACTAL_NONE);
	BIND_ENUM_CONSTANT(FRACTAL_FBM);
	BIND_ENUM_CONSTANT(FRACTAL_RIDGED);
	BIND_ENUM_CONSTANT(FRACTAL_PING_PONG);

	// BIND_ENUM_CONSTANT(ROTATION_3D_NONE);
	// BIND_ENUM_CONSTANT(ROTATION_3D_IMPROVE_XY_PLANES);
	// BIND_ENUM_CONSTANT(ROTATION_3D_IMPROVE_XZ_PLANES);

	BIND_ENUM_CONSTANT(CELLULAR_DISTANCE_EUCLIDEAN);
	BIND_ENUM_CONSTANT(CELLULAR_DISTANCE_EUCLIDEAN_SQ);
	BIND_ENUM_CONSTANT(CELLULAR_DISTANCE_MANHATTAN);
	BIND_ENUM_CONSTANT(CELLULAR_DISTANCE_HYBRID);
	BIND_ENUM_CONSTANT(CELLULAR_DISTANCE_MAX_AXIS);

	BIND_ENUM_CONSTANT(CELLULAR_RETURN_INDEX_0);
	BIND_ENUM_CONSTANT(CELLULAR_RETURN_INDEX_0_ADD_1);
	BIND_ENUM_CONSTANT(CELLULAR_RETURN_INDEX_0_SUB_1);
	BIND_ENUM_CONSTANT(CELLULAR_RETURN_INDEX_0_MUL_1);
	BIND_ENUM_CONSTANT(CELLULAR_RETURN_INDEX_0_DIV_1);

	BIND_ENUM_CONSTANT(SIMD_NULL);
	BIND_ENUM_CONSTANT(SIMD_SCALAR);
	BIND_ENUM_CONSTANT(SIMD_SSE);
	BIND_ENUM_CONSTANT(SIMD_SSE2);
	BIND_ENUM_CONSTANT(SIMD_SSE3);
	BIND_ENUM_CONSTANT(SIMD_SSSE3);
	BIND_ENUM_CONSTANT(SIMD_SSE41);
	BIND_ENUM_CONSTANT(SIMD_SSE42);
	BIND_ENUM_CONSTANT(SIMD_AVX);
	BIND_ENUM_CONSTANT(SIMD_AVX2);
	BIND_ENUM_CONSTANT(SIMD_AVX512);
	BIND_ENUM_CONSTANT(SIMD_NEON);
}

} // namespace zylann
