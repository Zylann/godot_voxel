#ifndef VOXEL_GRAPH_FUNCTION_H
#define VOXEL_GRAPH_FUNCTION_H

#include "../../util/godot/classes/resource.h"
#include "program_graph.h"

namespace zylann::voxel::pg {

// Generic processing graph made of operation nodes.
// TODO This class had to be prefixed `VoxelGraph` but I wished it was just `pg::Function`.
// It is that way because Godot classes have no namespaces.
// This class is progressively becoming more generic, it doesn't have much relation to voxels (but is useful for
// processing data).
class VoxelGraphFunction : public Resource {
	GDCLASS(VoxelGraphFunction, Resource)
public:
	static const char *SIGNAL_NODE_NAME_CHANGED;

	// Node indexes within the DB.
	// Don't use these in saved data,
	// they can change depending on which features the module is compiled with.
	enum NodeTypeID {
		NODE_CONSTANT,
		NODE_INPUT_X,
		NODE_INPUT_Y,
		NODE_INPUT_Z,
		NODE_OUTPUT_SDF,
		NODE_ADD,
		NODE_SUBTRACT,
		NODE_MULTIPLY,
		NODE_DIVIDE,
		NODE_SIN,
		NODE_FLOOR,
		NODE_ABS,
		NODE_SQRT,
		NODE_FRACT,
		NODE_STEPIFY,
		NODE_WRAP,
		NODE_MIN,
		NODE_MAX,
		NODE_DISTANCE_2D,
		NODE_DISTANCE_3D,
		NODE_CLAMP,
		NODE_CLAMP_C,
		NODE_MIX,
		NODE_REMAP,
		NODE_SMOOTHSTEP,
		NODE_CURVE,
		NODE_SELECT,
		NODE_NOISE_2D,
		NODE_NOISE_3D,
		NODE_IMAGE_2D,
		NODE_SDF_PLANE,
		NODE_SDF_BOX,
		NODE_SDF_SPHERE,
		NODE_SDF_TORUS,
		NODE_SDF_PREVIEW, // For debugging
		NODE_SDF_SPHERE_HEIGHTMAP,
		NODE_SDF_SMOOTH_UNION,
		NODE_SDF_SMOOTH_SUBTRACT,
		NODE_NORMALIZE_3D,
		NODE_FAST_NOISE_2D,
		NODE_FAST_NOISE_3D,
		NODE_FAST_NOISE_GRADIENT_2D,
		NODE_FAST_NOISE_GRADIENT_3D,
		NODE_OUTPUT_WEIGHT,
		NODE_OUTPUT_TYPE,
#ifdef VOXEL_ENABLE_FAST_NOISE_2
		NODE_FAST_NOISE_2_2D,
		NODE_FAST_NOISE_2_3D,
#endif
		NODE_OUTPUT_SINGLE_TEXTURE,
		NODE_EXPRESSION,
		NODE_POWI, // pow(x, constant positive integer)
		NODE_POW, // pow(x, y)
		NODE_INPUT_SDF,
		NODE_COMMENT,
		NODE_FUNCTION,
		NODE_CUSTOM_INPUT,
		NODE_CUSTOM_OUTPUT,
		NODE_RELAY,
		NODE_SPOTS_2D,
		NODE_SPOTS_3D,

		NODE_TYPE_COUNT
	};

	struct Port {
		NodeTypeID type;
		// Used for port types that can appear multiple times, but with different indices.
		// Initially used for OutputWeight nodes.
		unsigned int sub_index = 0;
		// Name of the port. If the port is custom, it identifies it (it doesn't matter otherwise).
		String name;

		Port() {}
		Port(NodeTypeID p_type, const String &p_name) : type(p_type), name(p_name) {}

		inline bool is_custom() const {
			return type == NODE_CUSTOM_INPUT || type == NODE_CUSTOM_OUTPUT;
		}

		inline bool equals(const Port &other) const {
			if (is_custom()) {
				return name == other.name;
			} else {
				return type == other.type && sub_index == other.sub_index;
			}
		}
	};

	void clear();

	// Graph edition API
	// Important: functions editing the graph are NOT thread-safe.
	// They are expected to be used by the main thread (editor or game logic).

	uint32_t create_node(NodeTypeID type_id, Vector2 position, uint32_t id = ProgramGraph::NULL_ID);
	void remove_node(uint32_t node_id);

	uint32_t create_function_node(
			Ref<VoxelGraphFunction> func, Vector2 position, uint32_t p_id = ProgramGraph::NULL_ID);

	// Checks if the specified connection can be created
	bool can_connect(
			uint32_t src_node_id, uint32_t src_port_index, uint32_t dst_node_id, uint32_t dst_port_index) const;

	// Checks if the specified connection is valid (without considering existing connections)
	bool is_valid_connection(
			uint32_t src_node_id, uint32_t src_port_index, uint32_t dst_node_id, uint32_t dst_port_index) const;

	void add_connection(uint32_t src_node_id, uint32_t src_port_index, uint32_t dst_node_id, uint32_t dst_port_index);
	void remove_connection(
			uint32_t src_node_id, uint32_t src_port_index, uint32_t dst_node_id, uint32_t dst_port_index);
	void get_connections(std::vector<ProgramGraph::Connection> &connections) const;

	// Finds which source port is connected to the given destination.
	// Returns false if `dst` has no inbound connection.
	bool try_get_connection_to(ProgramGraph::PortLocation dst, ProgramGraph::PortLocation &out_src) const;

	bool has_node(uint32_t node_id) const;

	void set_node_name(uint32_t node_id, StringName name);
	StringName get_node_name(uint32_t node_id) const;
	uint32_t find_node_by_name(StringName name) const;

	Variant get_node_param(uint32_t node_id, uint32_t param_index) const;
	void set_node_param(uint32_t node_id, uint32_t param_index, Variant value);

	static bool get_expression_variables(std::string_view code, std::vector<std::string_view> &vars);
	void get_expression_node_inputs(uint32_t node_id, std::vector<std::string> &out_names) const;
	void set_expression_node_inputs(uint32_t node_id, PackedStringArray names);

	Variant get_node_default_input(uint32_t node_id, uint32_t input_index) const;
	void set_node_default_input(uint32_t node_id, uint32_t input_index, Variant value);

	bool get_node_default_inputs_autoconnect(uint32_t node_id) const;
	void set_node_default_inputs_autoconnect(uint32_t node_id, bool enabled);

	Vector2 get_node_gui_position(uint32_t node_id) const;
	void set_node_gui_position(uint32_t node_id, Vector2 pos);

	Vector2 get_node_gui_size(uint32_t node_id) const;
	void set_node_gui_size(uint32_t node_id, Vector2 size);

	NodeTypeID get_node_type_id(uint32_t node_id) const;
	PackedInt32Array get_node_ids() const;
	uint32_t generate_node_id() {
		return _graph.generate_node_id();
	}

	unsigned int get_nodes_count() const;

	// Editor

#ifdef TOOLS_ENABLED
	void get_configuration_warnings(PackedStringArray &out_warnings) const;

	// Gets a hash that attempts to only change if the output of the graph is different.
	// This is computed from the editable graph data, not the compiled result.
	uint64_t get_output_graph_hash() const;
#endif

	// Internal

	const ProgramGraph &get_graph() const;
	void find_dependencies(uint32_t node_id, std::vector<uint32_t> &out_dependencies) const;
	Dictionary get_graph_as_variant_data() const;
	bool load_graph_from_variant_data(Dictionary data);

	unsigned int get_node_input_count(uint32_t node_id) const;
	unsigned int get_node_output_count(uint32_t node_id) const;

	// TODO Should this be directly a node type?
	enum AutoConnect { //
		AUTO_CONNECT_NONE,
		AUTO_CONNECT_X,
		AUTO_CONNECT_Y,
		AUTO_CONNECT_Z
	};

	static bool try_get_node_type_id_from_auto_connect(AutoConnect ac, NodeTypeID &out_node_type);
	static bool try_get_auto_connect_from_node_type_id(NodeTypeID node_type, AutoConnect &out_ac);

	void get_node_input_info(
			uint32_t node_id, unsigned int input_index, String *out_name, AutoConnect *out_autoconnect) const;
	String get_node_output_name(uint32_t node_id, unsigned int output_index) const;
	Span<const Port> get_input_definitions() const;
	Span<const Port> get_output_definitions() const;
	// Currently used for testing
	void set_io_definitions(Span<const Port> inputs, Span<const Port> outputs);
	bool contains_reference_to_function(Ref<VoxelGraphFunction> p_func, int max_recursion = 16) const;
	bool contains_reference_to_function(const VoxelGraphFunction &p_func, int max_recursion = 16) const;
	void auto_pick_inputs_and_outputs();

	bool get_node_input_index_by_name(uint32_t node_id, String name, unsigned int &out_input_index) const;
	bool get_node_param_index_by_name(uint32_t node_id, String name, unsigned int &out_param_index) const;

	void update_function_nodes(std::vector<ProgramGraph::Connection> *removed_connections);

private:
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
	void _b_set_node_name(int node_id, String name);

	Array _b_get_input_definitions() const;
	void _b_set_input_definitions(Array data);

	Array _b_get_output_definitions() const;
	void _b_set_output_definitions(Array data);

	static void _bind_methods();

	ProgramGraph _graph;
	std::vector<Port> _inputs;
	std::vector<Port> _outputs;
};

ProgramGraph::Node *create_node_internal(ProgramGraph &graph, VoxelGraphFunction::NodeTypeID type_id, Vector2 position,
		uint32_t id, bool create_default_instances);

void auto_pick_input_and_outputs(const ProgramGraph &graph, std::vector<VoxelGraphFunction::Port> &inputs,
		std::vector<VoxelGraphFunction::Port> &outputs);

Array serialize_io_definitions(Span<const VoxelGraphFunction::Port> ports);

#ifdef TOOLS_ENABLED

inline String get_port_display_name(const VoxelGraphFunction::Port &port) {
	return port.name.is_empty() ? String("<unnamed>") : port.name;
}

#endif

} // namespace zylann::voxel::pg

VARIANT_ENUM_CAST(zylann::voxel::pg::VoxelGraphFunction::NodeTypeID)

#endif // VOXEL_GRAPH_FUNCTION_H
