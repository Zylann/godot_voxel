#ifndef VOXEL_FAST_NOISE_2_H
#define VOXEL_FAST_NOISE_2_H

#include "../math/interval.h"
#include "../span.h"
#include "FastNoise/FastNoise.h"
#include <core/io/resource.h>

class Image;

namespace zylann {

// Can't call it FastNoise? because FastNoise is a namespace already
class FastNoise2 : public Resource {
	GDCLASS(FastNoise2, Resource)
public:
	static const int MAX_OCTAVES = 32;
	// This minimum size exists to fix an issue with SIMD operations, which need to use more than one element.
	static const unsigned int MIN_BUFFER_SIZE = 16;

	enum SIMDLevel {
		SIMD_NULL = FastSIMD::Level_Null, // Uninitilised
		SIMD_SCALAR = FastSIMD::Level_Scalar, // 80386 instruction set (Not SIMD)
		SIMD_SSE = FastSIMD::Level_SSE, // SSE (XMM) supported by CPU (not testing for O.S. support)
		SIMD_SSE2 = FastSIMD::Level_SSE2, // SSE2
		SIMD_SSE3 = FastSIMD::Level_SSE3, // SSE3
		SIMD_SSSE3 = FastSIMD::Level_SSSE3, // Supplementary SSE3 (SSSE3)
		SIMD_SSE41 = FastSIMD::Level_SSE41, // SSE4.1
		SIMD_SSE42 = FastSIMD::Level_SSE42, // SSE4.2
		SIMD_AVX = FastSIMD::Level_AVX, // AVX supported by CPU and operating system
		SIMD_AVX2 = FastSIMD::Level_AVX2, // AVX2
		SIMD_AVX512 = FastSIMD::Level_AVX512, // AVX512, AVX512DQ supported by CPU and operating system

		SIMD_NEON = FastSIMD::Level_NEON, // ARM NEON
	};

	enum NoiseType { //
		TYPE_OPEN_SIMPLEX_2 = 0,
		TYPE_SIMPLEX,
		TYPE_PERLIN,
		TYPE_VALUE,
		TYPE_CELLULAR,
		// Special type overriding most options with a tree made in Auburn's NoiseTool
		TYPE_ENCODED_NODE_TREE
		// TODO Implement NoiseTool graph editor inside Godot?
		//TYPE_NODE_TREE,
	};

	static constexpr const char *NOISE_TYPE_HINT_STRING = "OpenSimplex2,Simplex,Perlin,Value,Cellular,EncodedNodeTree";

	enum FractalType { //
		FRACTAL_NONE = 0,
		FRACTAL_FBM,
		FRACTAL_RIDGED,
		FRACTAL_PING_PONG
	};

	static constexpr const char *FRACTAL_TYPE_HINT_STRING = "None,FBm,Ridged,PingPong";

	enum CellularDistanceFunction { //
		CELLULAR_DISTANCE_EUCLIDEAN = (int)FastNoise::DistanceFunction::Euclidean,
		CELLULAR_DISTANCE_EUCLIDEAN_SQ = (int)FastNoise::DistanceFunction::EuclideanSquared,
		CELLULAR_DISTANCE_MANHATTAN = (int)FastNoise::DistanceFunction::Manhattan,
		CELLULAR_DISTANCE_HYBRID = (int)FastNoise::DistanceFunction::Hybrid,
		CELLULAR_DISTANCE_MAX_AXIS = (int)FastNoise::DistanceFunction::MaxAxis
	};

	static constexpr const char *CELLULAR_DISTANCE_FUNCTION_HINT_STRING =
			"Euclidean,EuclideanSq,Manhattan,Hybrid,MaxAxis";

	enum CellularReturnType { //
		CELLULAR_RETURN_INDEX_0 = (int)FastNoise::CellularDistance::ReturnType::Index0,
		CELLULAR_RETURN_INDEX_0_ADD_1 = (int)FastNoise::CellularDistance::ReturnType::Index0Add1,
		CELLULAR_RETURN_INDEX_0_SUB_1 = (int)FastNoise::CellularDistance::ReturnType::Index0Sub1,
		CELLULAR_RETURN_INDEX_0_MUL_1 = (int)FastNoise::CellularDistance::ReturnType::Index0Mul1,
		CELLULAR_RETURN_INDEX_0_DIV_1 = (int)FastNoise::CellularDistance::ReturnType::Index0Div1
	};

	static constexpr const char *CELLULAR_RETURN_TYPE_HINT_STRING =
			"Index0,Index0Add1,Index0Sub1,Index0Mul1,Index0Div1";

	FastNoise2();

	SIMDLevel get_simd_level() const;
	static String get_simd_level_name(SIMDLevel level);

	void set_seed(int seed);
	int get_seed() const;

	void set_noise_type(NoiseType type);
	NoiseType get_noise_type() const;

	void set_period(float p);
	float get_period() const;

	// Fractal

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

	// Terrace modifier

	void set_terrace_enabled(bool enable);
	bool is_terrace_enabled() const;

	void set_terrace_multiplier(float m);
	float get_terrace_multiplier() const;

	void set_terrace_smoothness(float s);
	float get_terrace_smoothness() const;

	// Remap

	void set_remap_enabled(bool enabled);
	bool is_remap_enabled() const;

	void set_remap_input_min(float min_value);
	float get_remap_input_min() const;

	void set_remap_input_max(float max_value);
	float get_remap_input_max() const;

	void set_remap_output_min(float min_value);
	float get_remap_output_min() const;

	void set_remap_output_max(float max_value);
	float get_remap_output_max() const;

	// Cellular

	void set_cellular_distance_function(CellularDistanceFunction cdf);
	CellularDistanceFunction get_cellular_distance_function() const;

	void set_cellular_return_type(CellularReturnType rt);
	CellularReturnType get_cellular_return_type() const;

	void set_cellular_jitter(float jitter);
	float get_cellular_jitter() const;

	// Misc

	void set_encoded_node_tree(String data);
	String get_encoded_node_tree() const;

	void update_generator();
	bool is_valid() const;

	// Queries
	// TODO Double-precision support. FastNoise2 doesn't have it yet, so it's all `float` for now.

	float get_noise_2d_single(Vector2 pos) const;
	float get_noise_3d_single(Vector3 pos) const;

	void get_noise_2d_series(Span<const float> src_x, Span<const float> src_y, Span<float> dst) const;
	void get_noise_3d_series(
			Span<const float> src_x, Span<const float> src_y, Span<const float> src_z, Span<float> dst) const;

	void get_noise_2d_grid(Vector2 origin, Vector2i size, Span<float> dst) const;
	void get_noise_3d_grid(Vector3 origin, Vector3i size, Span<float> dst) const;

	void get_noise_2d_grid_tileable(Vector2i size, Span<float> dst) const;

	void generate_image(Ref<Image> image, bool tileable) const;

	math::Interval get_estimated_output_range() const;

private:
	// Non-static method for scripts because Godot4 does not support binding static methods (it's only
	// implemented for primitive types)
	// TODO Make it static, it is supported now
	String _b_get_simd_level_name(SIMDLevel level);

	static void _bind_methods();

	int _seed = 1337;

	NoiseType _noise_type = TYPE_OPEN_SIMPLEX_2;
	String _last_set_encoded_node_tree;

	float _period = 64.f;

	FractalType _fractal_type = FRACTAL_NONE;
	int _fractal_octaves = 3;
	float _fractal_lacunarity = 2.f;
	float _fractal_gain = 0.5f;
	float _fractal_ping_pong_strength = 2.f;

	bool _terrace_enabled = false;
	float _terrace_multiplier = 1.0;
	float _terrace_smoothness = 0.0;

	CellularDistanceFunction _cellular_distance_function = CELLULAR_DISTANCE_EUCLIDEAN;
	CellularReturnType _cellular_return_type = CELLULAR_RETURN_INDEX_0;
	float _cellular_jitter = 1.0;

	bool _remap_enabled = false;
	float _remap_src_min = -1.0;
	float _remap_src_max = 1.0;
	float _remap_dst_min = -1.0;
	float _remap_dst_max = 1.0;

	FastNoise::SmartNode<> _generator;
};

} // namespace zylann

VARIANT_ENUM_CAST(zylann::FastNoise2::SIMDLevel);
VARIANT_ENUM_CAST(zylann::FastNoise2::NoiseType);
VARIANT_ENUM_CAST(zylann::FastNoise2::FractalType);
VARIANT_ENUM_CAST(zylann::FastNoise2::CellularDistanceFunction);
VARIANT_ENUM_CAST(zylann::FastNoise2::CellularReturnType);

#endif // VOXEL_FAST_NOISE_2_H
