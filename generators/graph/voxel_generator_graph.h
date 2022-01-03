#ifndef VOXEL_GENERATOR_GRAPH_H
#define VOXEL_GENERATOR_GRAPH_H

#include "../voxel_generator.h"
#include "program_graph.h"
#include "voxel_graph_runtime.h"
#include <memory>

class VoxelGeneratorGraph : public VoxelGenerator {
	GDCLASS(VoxelGeneratorGraph, VoxelGenerator)
public:
	static const char *SIGNAL_NODE_NAME_CHANGED;

	enum NodeTypeID {
		NODE_CONSTANT = 0,
		NODE_INPUT_X = 1,
		NODE_INPUT_Y = 2,
		NODE_INPUT_Z = 3,
		NODE_OUTPUT_SDF = 4,
		NODE_ADD = 5,
		NODE_SUBTRACT = 6,
		NODE_MULTIPLY = 7,
		NODE_DIVIDE = 8,
		NODE_SIN = 9,
		NODE_FLOOR = 10,
		NODE_ABS = 11,
		NODE_SQRT = 12,
		NODE_FRACT = 13,
		NODE_STEPIFY = 14,
		NODE_WRAP = 15,
		NODE_MIN = 16,
		NODE_MAX = 17,
		NODE_DISTANCE_2D = 18,
		NODE_DISTANCE_3D = 19,
		NODE_CLAMP = 20,
		NODE_MIX = 21,
		NODE_REMAP = 22,
		NODE_SMOOTHSTEP = 23,
		NODE_CURVE = 24,
		NODE_SELECT = 25,
		NODE_NOISE_2D = 26,
		NODE_NOISE_3D = 27,
		NODE_IMAGE_2D = 28,
		NODE_SDF_PLANE = 29,
		NODE_SDF_BOX = 30,
		NODE_SDF_SPHERE = 31,
		NODE_SDF_TORUS = 32,
		NODE_SDF_PREVIEW = 33, // For debugging
		NODE_SDF_SPHERE_HEIGHTMAP = 34,
		NODE_SDF_SMOOTH_UNION = 35,
		NODE_SDF_SMOOTH_SUBTRACT = 36,
		NODE_NORMALIZE_3D = 37,
		NODE_FAST_NOISE_2D = 38,
		NODE_FAST_NOISE_3D = 39,
		NODE_FAST_NOISE_GRADIENT_2D = 40,
		NODE_FAST_NOISE_GRADIENT_3D = 41,
		NODE_OUTPUT_WEIGHT = 42,
#ifdef VOXEL_ENABLE_FAST_NOISE_2
		NODE_FAST_NOISE_2_2D = 43,
		NODE_FAST_NOISE_2_3D = 44,
#endif

#ifdef VOXEL_ENABLE_FAST_NOISE_2
		NODE_TYPE_COUNT = 45
#else
		NODE_TYPE_COUNT = 43
#endif
	};

	VoxelGeneratorGraph();
	~VoxelGeneratorGraph();

	void clear();

	// Graph edition API
	// Important: functions editing the graph are NOT thread-safe.
	// They are expected to be used by the main thread (editor or game logic).

	uint32_t create_node(NodeTypeID type_id, Vector2 position, uint32_t id = zylann::ProgramGraph::NULL_ID);
	void remove_node(uint32_t node_id);

	bool can_connect(
			uint32_t src_node_id, uint32_t src_port_index, uint32_t dst_node_id, uint32_t dst_port_index) const;
	void add_connection(uint32_t src_node_id, uint32_t src_port_index, uint32_t dst_node_id, uint32_t dst_port_index);
	void remove_connection(
			uint32_t src_node_id, uint32_t src_port_index, uint32_t dst_node_id, uint32_t dst_port_index);
	void get_connections(std::vector<zylann::ProgramGraph::Connection> &connections) const;
	bool try_get_connection_to(
			zylann::ProgramGraph::PortLocation dst, zylann::ProgramGraph::PortLocation &out_src) const;

	bool has_node(uint32_t node_id) const;

	void set_node_name(uint32_t node_id, StringName name);
	StringName get_node_name(uint32_t node_id) const;
	uint32_t find_node_by_name(StringName name) const;

	Variant get_node_param(uint32_t node_id, uint32_t param_index) const;
	void set_node_param(uint32_t node_id, uint32_t param_index, Variant value);

	Variant get_node_default_input(uint32_t node_id, uint32_t input_index) const;
	void set_node_default_input(uint32_t node_id, uint32_t input_index, Variant value);

	Vector2 get_node_gui_position(uint32_t node_id) const;
	void set_node_gui_position(uint32_t node_id, Vector2 pos);

	NodeTypeID get_node_type_id(uint32_t node_id) const;
	PackedInt32Array get_node_ids() const;
	uint32_t generate_node_id() {
		return _graph.generate_node_id();
	}

	int get_nodes_count() const;

	void load_plane_preset();

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

	Result generate_block(VoxelBlockRequest &input) override;
	//float generate_single(const Vector3i &position);
	bool supports_single_generation() const override {
		return true;
	}
	VoxelSingleValue generate_single(Vector3i position, unsigned int channel) override;

	Ref<Resource> duplicate(bool p_subresources) const override;

	// Utility

	void bake_sphere_bumpmap(Ref<Image> im, float ref_radius, float min_height, float max_height);
	void bake_sphere_normalmap(Ref<Image> im, float ref_radius, float strength);

	// Internal

	zylann::voxel::VoxelGraphRuntime::CompilationResult compile();
	bool is_good() const;

	void generate_set(Span<float> in_x, Span<float> in_y, Span<float> in_z);

	// Returns state from the last generator used in the current thread
	static const zylann::voxel::VoxelGraphRuntime::State &get_last_state_from_current_thread();
	static Span<const int> get_last_execution_map_debug_from_current_thread();

	bool try_get_output_port_address(zylann::ProgramGraph::PortLocation port, uint32_t &out_address) const;

	void find_dependencies(uint32_t node_id, std::vector<uint32_t> &out_dependencies) const;

	// Debug

	zylann::math::Interval debug_analyze_range(Vector3i min_pos, Vector3i max_pos, bool optimize_execution_map) const;
	float debug_measure_microseconds_per_voxel(bool singular);
	void debug_load_waves_preset();

private:
	Dictionary get_graph_as_variant_data() const;
	void load_graph_from_variant_data(Dictionary data);
	void register_subresource(Resource &resource);
	void unregister_subresource(Resource &resource);
	void register_subresources();
	void unregister_subresources();
	void _on_subresource_changed();

	int _b_get_node_type_count() const;
	Dictionary _b_get_node_type_info(int type_id) const;
	Array _b_get_connections() const;
	// TODO Only exists because the UndoRedo API is confusing `null` with `absence of argument`...
	// See https://github.com/godotengine/godot/issues/36895
	void _b_set_node_param_null(int node_id, int param_index);
	float _b_generate_single(Vector3 pos);
	Vector2 _b_debug_analyze_range(Vector3 min_pos, Vector3 max_pos) const;
	Dictionary _b_compile();

	struct WeightOutput {
		unsigned int layer_index;
		unsigned int output_buffer_index;
	};

	static void gather_indices_and_weights(Span<const WeightOutput> weight_outputs,
			const zylann::voxel::VoxelGraphRuntime::State &state, Vector3i rmin, Vector3i rmax, int ry,
			VoxelBufferInternal &out_voxel_buffer, FixedArray<uint8_t, 4> spare_indices);

	static void _bind_methods();

	zylann::ProgramGraph _graph;
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

	struct Runtime {
		zylann::voxel::VoxelGraphRuntime runtime;
		// Indices that are not used in the graph.
		// This is used when there are less than 4 texture weight outputs.
		FixedArray<uint8_t, 4> spare_texture_indices;
		// Index to the SDF output
		int sdf_output_buffer_index = -1;
		FixedArray<WeightOutput, 16> weight_outputs;
		// List of indices to feed queries. The order doesn't matter, can be different from `weight_outputs`.
		FixedArray<unsigned int, 16> weight_output_indices;
		unsigned int weight_outputs_count = 0;
	};

	std::shared_ptr<Runtime> _runtime = nullptr;
	RWLock _runtime_lock;

	struct Cache {
		std::vector<float> x_cache;
		std::vector<float> y_cache;
		std::vector<float> z_cache;
		zylann::voxel::VoxelGraphRuntime::State state;
		zylann::voxel::VoxelGraphRuntime::ExecutionMap optimized_execution_map;
	};

	static thread_local Cache _cache;
};

VARIANT_ENUM_CAST(VoxelGeneratorGraph::NodeTypeID)

#endif // VOXEL_GENERATOR_GRAPH_H
