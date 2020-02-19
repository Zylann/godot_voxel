#ifndef VOXEL_GENERATOR_GRAPH_H
#define VOXEL_GENERATOR_GRAPH_H

#include "../voxel_generator.h"
#include "program_graph.h"
#include <modules/opensimplex/open_simplex_noise.h>

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

	void load_waves_preset();

	void generate_block(VoxelBlockRequest &input) override;
	float generate_single(const Vector3i &position);

	float debug_measure_microseconds_per_voxel();

private:
	void compile();

	static void _bind_methods();

	struct Node {
		NodeTypeID type;
		std::vector<Variant> params;
		Vector2 gui_position;
	};

	ProgramGraph _graph;
	HashMap<uint32_t, Node *> _nodes;

	std::vector<uint8_t> _program;
	std::vector<float> _memory;
	VoxelBuffer::ChannelId _channel = VoxelBuffer::CHANNEL_SDF;
	float _iso_scale = 0.1;
};

#endif // VOXEL_GENERATOR_GRAPH_H
