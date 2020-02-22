#ifndef VOXEL_GENERATOR_GRAPH_H
#define VOXEL_GENERATOR_GRAPH_H

#include "../../cube_tables.h"
#include "../../util/interval.h"
#include "../voxel_generator.h"
#include "program_graph.h"

class VoxelGeneratorGraph : public VoxelGenerator {
	GDCLASS(VoxelGeneratorGraph, VoxelGenerator)
public:
	enum NodeTypeID {
		NODE_CONSTANT,
		NODE_INPUT_X,
		NODE_INPUT_Y,
		NODE_INPUT_Z,
		NODE_OUTPUT_SDF,
		NODE_ADD,
		NODE_SUBTRACT,
		NODE_MULTIPLY,
		NODE_SINE,
		NODE_FLOOR,
		NODE_ABS,
		NODE_SQRT,
		NODE_DISTANCE_2D,
		NODE_DISTANCE_3D,
		NODE_CLAMP,
		NODE_MIX,
		NODE_REMAP,
		NODE_CURVE,
		NODE_NOISE_2D,
		NODE_NOISE_3D,
		NODE_IMAGE_2D,
		NODE_TYPE_COUNT
	};

	struct NodeTypeDB {

		struct Port {
			String name;

			Port(String p_name) :
					name(p_name) {}
		};

		struct Param {
			String name;
			Variant default_value;

			Param(String p_name, Variant p_default_value = Variant()) :
					name(p_name),
					default_value(p_default_value) {}
		};

		struct NodeType {
			std::vector<Port> inputs;
			std::vector<Port> outputs;
			std::vector<Param> params;
		};

		NodeTypeDB();

		static NodeTypeDB *get_singleton();
		static void create_singleton();
		static void destroy_singleton();

		FixedArray<NodeType, NODE_TYPE_COUNT> types;
	};

	VoxelGeneratorGraph();
	~VoxelGeneratorGraph();

	void clear();

	uint32_t create_node(NodeTypeID type_id);
	void remove_node(uint32_t node_id);
	void node_connect(ProgramGraph::PortLocation src, ProgramGraph::PortLocation dst);
	void node_set_param(uint32_t node_id, uint32_t param_index, Variant value);

	// TODO Empty preset
	void load_waves_preset();

	int get_used_channels_mask() const override;

	void generate_block(VoxelBlockRequest &input) override;
	float generate_single(const Vector3i &position);

	enum BoundsType {
		BOUNDS_NONE = 0,
		BOUNDS_VERTICAL,
		BOUNDS_BOX,
		BOUNDS_TYPE_COUNT
	};

	void clear_bounds();
	void set_vertical_bounds(int min_y, int max_y, float bottom_sdf_value, float top_sdf_value, uint64_t bottom_type_value, uint64_t top_type_value);
	void set_box_bounds(Vector3i min, Vector3i max, float sdf_value, uint64_t type_value);

	float debug_measure_microseconds_per_voxel();

private:
	void compile();
	Interval analyze_range(Vector3i min_pos, Vector3i max_pos);

	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	static void _bind_methods();

	struct Node {
		NodeTypeID type;
		std::vector<Variant> params;
		Vector2 gui_position;
	};

	ProgramGraph _graph;
	HashMap<uint32_t, Node *> _nodes;

	struct Bounds {
		BoundsType type = BOUNDS_NONE;
		Vector3i min;
		Vector3i max;
		// Voxel values beyond bounds
		float sdf_value0 = 1.f;
		float sdf_value1 = 1.f;
		uint64_t type_value0 = 0;
		uint64_t type_value1 = 0;
	};

	std::vector<uint8_t> _program;
	std::vector<float> _memory;
	VoxelBuffer::ChannelId _channel = VoxelBuffer::CHANNEL_SDF;
	float _iso_scale = 0.1;
	Bounds _bounds;
};

VARIANT_ENUM_CAST(VoxelGeneratorGraph::NodeTypeID)
VARIANT_ENUM_CAST(VoxelGeneratorGraph::BoundsType)

#endif // VOXEL_GENERATOR_GRAPH_H
