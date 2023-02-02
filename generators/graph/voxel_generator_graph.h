#ifndef VOXEL_GENERATOR_GRAPH_H
#define VOXEL_GENERATOR_GRAPH_H

#include "../../util/macros.h"
#include "../../util/thread/rw_lock.h"
#include "../voxel_generator.h"
#include "program_graph.h"
#include "voxel_graph_function.h"
#include "voxel_graph_runtime.h"

#include <memory>

ZN_GODOT_FORWARD_DECLARE(class Image)

namespace zylann::voxel {

// Uses an internal VoxelGraphFunction to generate voxel data.
class VoxelGeneratorGraph : public VoxelGenerator {
	GDCLASS(VoxelGeneratorGraph, VoxelGenerator)
public:
	static const char *SIGNAL_NODE_NAME_CHANGED;

	VoxelGeneratorGraph();
	~VoxelGeneratorGraph();

	void clear();
	void load_plane_preset();

	Ref<pg::VoxelGraphFunction> get_main_function() const;

	// Performance tuning (advanced)

	bool is_using_optimized_execution_map() const;
	void set_use_optimized_execution_map(bool use);

	float get_sdf_clip_threshold() const;
	void set_sdf_clip_threshold(float t);

	void set_use_subdivision(bool use);
	bool is_using_subdivision() const;

	void set_subdivision_size(int size);
	int get_subdivision_size() const;

	void set_debug_clipped_blocks(bool enabled);
	bool is_debug_clipped_blocks() const;

	void set_use_xz_caching(bool enabled);
	bool is_using_xz_caching() const;

	// VoxelGenerator implementation

	int get_used_channels_mask() const override;

	Result generate_block(VoxelGenerator::VoxelQueryData &input) override;
	// float generate_single(const Vector3i &position);
	bool supports_single_generation() const override {
		return true;
	}
	bool supports_series_generation() const override {
		return true;
	}
	VoxelSingleValue generate_single(Vector3i position, unsigned int channel) override;

	void generate_series(Span<const float> positions_x, Span<const float> positions_y, Span<const float> positions_z,
			unsigned int channel, Span<float> out_values, Vector3f min_pos, Vector3f max_pos) override;

	// Ref<Resource> duplicate(bool p_subresources) const ZN_OVERRIDE_UNLESS_GODOT_EXTENSION;

	// Utility

	void bake_sphere_bumpmap(Ref<Image> im, float ref_radius, float min_height, float max_height);
	void bake_sphere_normalmap(Ref<Image> im, float ref_radius, float strength);

	// Internal

	pg::CompilationResult compile(bool debug);
	bool is_good() const;

	void generate_set(Span<float> in_x, Span<float> in_y, Span<float> in_z);
	void generate_series(Span<float> in_x, Span<float> in_y, Span<float> in_z, Span<float> in_sdf);

	// Returns state from the last generator used in the current thread
	static const pg::Runtime::State &get_last_state_from_current_thread();
	static Span<const uint32_t> get_last_execution_map_debug_from_current_thread();

	bool try_get_output_port_address(ProgramGraph::PortLocation port, uint32_t &out_address) const;
	int get_sdf_output_port_address() const;

	// GPU support

	bool supports_shaders() const override {
		// To some extent. It might fail if the graph contains nodes that are not compatible.
		return true;
	}

	bool get_shader_source(ShaderSourceData &out_data) const override;

	// Debug

	math::Interval debug_analyze_range(Vector3i min_pos, Vector3i max_pos, bool optimize_execution_map) const;

	struct NodeProfilingInfo {
		uint32_t node_id;
		uint32_t microseconds;
	};

	float debug_measure_microseconds_per_voxel(bool singular, std::vector<NodeProfilingInfo> *node_profiling_info);

	void debug_load_waves_preset();

	// Editor

#ifdef TOOLS_ENABLED
	void get_configuration_warnings(PackedStringArray &out_warnings) const override;
#endif

private:
	void _on_subresource_changed();
	float _b_generate_single(Vector3 pos);
	Vector2 _b_debug_analyze_range(Vector3 min_pos, Vector3 max_pos) const;
	Dictionary _b_compile();
	float _b_debug_measure_microseconds_per_voxel(bool singular);
	Dictionary get_graph_as_variant_data() const;
	void load_graph_from_variant_data(Dictionary data);

	struct WeightOutput {
		unsigned int layer_index;
		unsigned int output_buffer_index;
	};

	static void gather_indices_and_weights(Span<const WeightOutput> weight_outputs, const pg::Runtime::State &state,
			Vector3i rmin, Vector3i rmax, int ry, VoxelBufferInternal &out_voxel_buffer,
			FixedArray<uint8_t, 4> spare_indices);

	static void _bind_methods();

	Ref<pg::VoxelGraphFunction> _main_function;

	// This generator performs range analysis using nodes of the graph. Terrain surface can only appear when SDF
	// crosses zero within a block. For each generated block, an estimated range of the output is calculated.
	// If that range is beyond this threshold (either negatively or positively), then blocks will be given a uniform
	// value, either air or matter, skipping generation of all voxels.
	// Setting a high threshold turns it off, providing consistent SDF, but it may severely impact performance.
	float _sdf_clip_threshold = 1.5f;
	// Sometimes block size can be larger, but it makes range analysis less precise. So it is possible to subdivide
	// generation within areas of the block instead of doing it whole.
	// Blocks size must be a multiple of the subdivision size.
	bool _use_subdivision = true;
	int _subdivision_size = 16;
	// When enabled, the generator will attempt to optimize out nodes that don't need to run in specific areas,
	// if their output range is considered to not affect the final result.
	bool _use_optimized_execution_map = true;
	// When enabled, nodes using only the X and Z coordinates will be cached when generating blocks in slices along Y.
	// This prevents recalculating values that would otherwise be the same on each slice.
	// It helps a lot when part of the graph is generating a heightmap for example.
	bool _use_xz_caching = true;
	// If true, inverts clipped blocks so they create visual artifacts making the clipped area visible.
	bool _debug_clipped_blocks = false;

	// Only compiling and generation methods are thread-safe.

	// Wrapper around the runtime with extra information specialized for the use case
	struct Runtime {
		pg::Runtime runtime;
		// Indices that are not used in the graph.
		// This is used when there are less than 4 texture weight outputs.
		FixedArray<uint8_t, 4> spare_texture_indices;

		int x_input_index = -1;
		int y_input_index = -1;
		int z_input_index = -1;
		int sdf_input_index = -1;

		int sdf_output_index = -1;
		int sdf_output_buffer_index = -1;

		int type_output_index = -1;
		int type_output_buffer_index = -1;

		int single_texture_output_index = -1;
		int single_texture_output_buffer_index = -1;

		FixedArray<WeightOutput, 16> weight_outputs;
		// List of indices to feed queries. The order doesn't matter, can be different from `weight_outputs`.
		FixedArray<unsigned int, 16> weight_output_indices;
		unsigned int weight_outputs_count = 0;
	};

	// Helper to setup inputs for runtime queries
	template <typename T>
	struct QueryInputs {
		FixedArray<T, 4> query_inputs;
		unsigned int input_count = 0;

		inline QueryInputs(const Runtime &runtime_wrapper, T x, T y, T z, T sdf) {
			if (runtime_wrapper.x_input_index != -1) {
				query_inputs[runtime_wrapper.x_input_index] = x;
			}
			if (runtime_wrapper.y_input_index != -1) {
				query_inputs[runtime_wrapper.y_input_index] = y;
			}
			if (runtime_wrapper.z_input_index != -1) {
				query_inputs[runtime_wrapper.z_input_index] = z;
			}
			if (runtime_wrapper.sdf_input_index != -1) {
				query_inputs[runtime_wrapper.sdf_input_index] = sdf;
			}
			input_count = runtime_wrapper.runtime.get_input_count();
		}

		inline Span<T> get() {
			return to_span(query_inputs, input_count);
		}
	};

	std::shared_ptr<Runtime> _runtime = nullptr;
	RWLock _runtime_lock;

	struct Cache {
		std::vector<float> x_cache;
		std::vector<float> y_cache;
		std::vector<float> z_cache;
		std::vector<float> input_sdf_slice_cache;
		std::vector<float> input_sdf_full_cache;
		pg::Runtime::State state;
		pg::Runtime::ExecutionMap optimized_execution_map;
	};

	static Cache &get_tls_cache();
};

} // namespace zylann::voxel

#endif // VOXEL_GENERATOR_GRAPH_H
