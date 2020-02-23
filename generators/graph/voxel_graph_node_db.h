#ifndef VOXEL_GRAPH_NODE_DB_H
#define VOXEL_GRAPH_NODE_DB_H

#include "voxel_generator_graph.h"
#include <core/object.h>

class VoxelGraphNodeDB : public Object {
	GDCLASS(VoxelGraphNodeDB, Object)
public:
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

	VoxelGraphNodeDB();

	static VoxelGraphNodeDB *get_singleton();
	static void create_singleton();
	static void destroy_singleton();

	FixedArray<NodeType, VoxelGeneratorGraph::NODE_TYPE_COUNT> types;
};

#endif // VOXEL_GRAPH_NODE_DB_H
