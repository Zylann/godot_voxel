#ifndef VOXEL_GRAPH_NODE_DB_H
#define VOXEL_GRAPH_NODE_DB_H

#include "voxel_generator_graph.h"

class VoxelGraphNodeDB {
public:
	struct Port {
		String name;
		Variant default_value;
		//PortType port_type;

		Port(String p_name) :
				name(p_name),
				default_value(0.f) {}

		Port(String p_name, float p_default_value) :
				name(p_name),
				default_value(p_default_value) {}
	};

	struct Param {
		String name;
		Variant default_value;
		Variant::Type type;
		String class_name;
		uint32_t index = -1;
		bool has_range = false;
		int min_value;
		int max_value;

		Param(String p_name, Variant::Type p_type, Variant p_default_value = Variant()) :
				name(p_name),
				default_value(p_default_value),
				type(p_type) {}

		Param(String p_name, String p_class_name) :
				name(p_name),
				type(Variant::OBJECT),
				class_name(p_class_name) {}
	};

	enum Category {
		CATEGORY_INPUT = 0,
		CATEGORY_OUTPUT,
		CATEGORY_MATH,
		CATEGORY_CONVERT,
		CATEGORY_GENERATE,
		CATEGORY_SDF,
		CATEGORY_DEBUG,
		CATEGORY_COUNT
	};

	struct NodeType {
		String name;
		bool debug_only = false;
		Category category;
		std::vector<Port> inputs;
		std::vector<Port> outputs;
		std::vector<Param> params;
		HashMap<String, uint32_t> param_name_to_index;
		HashMap<String, uint32_t> input_name_to_index;
		VoxelGraphRuntime::CompileFunc compile_func = nullptr;
		VoxelGraphRuntime::ProcessBufferFunc process_buffer_func = nullptr;
		VoxelGraphRuntime::RangeAnalysisFunc range_analysis_func = nullptr;
	};

	VoxelGraphNodeDB();

	static VoxelGraphNodeDB *get_singleton();
	static void create_singleton();
	static void destroy_singleton();

	static const char *get_category_name(Category category);

	int get_type_count() const { return _types.size(); }
	bool is_valid_type_id(int type_id) const { return type_id >= 0 && type_id < static_cast<int>(_types.size()); }
	const NodeType &get_type(uint32_t id) const { return _types[id]; }
	Dictionary get_type_info_dict(uint32_t id) const;
	bool try_get_type_id_from_name(const String &name, VoxelGeneratorGraph::NodeTypeID &out_type_id) const;
	bool try_get_param_index_from_name(uint32_t type_id, const String &name, uint32_t &out_param_index) const;
	bool try_get_input_index_from_name(uint32_t type_id, const String &name, uint32_t &out_input_index) const;

private:
	FixedArray<NodeType, VoxelGeneratorGraph::NODE_TYPE_COUNT> _types;
	HashMap<String, VoxelGeneratorGraph::NodeTypeID> _type_name_to_id;
};

#endif // VOXEL_GRAPH_NODE_DB_H
