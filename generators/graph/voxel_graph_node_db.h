#ifndef VOXEL_GRAPH_NODE_DB_H
#define VOXEL_GRAPH_NODE_DB_H

#include "../../util/expression_parser.h"
#include "../../util/godot/string.h" // For String hash
#include "voxel_generator_graph.h"
#include "voxel_graph_compiler.h"
#include "voxel_graph_shader_generator.h"

namespace zylann::voxel {

class VoxelGraphNodeDB {
public:
	enum AutoConnect { //
		AUTO_CONNECT_NONE,
		AUTO_CONNECT_X,
		AUTO_CONNECT_Y,
		AUTO_CONNECT_Z
	};

	struct Port {
		String name;
		// Only relevant for inputs.
		Variant default_value;
		// Which connection will be automatically made if the input port is not connected and no fixed value has been
		// explicitely specified. Only relevant for inputs.
		AutoConnect auto_connect = AUTO_CONNECT_NONE;
		//PortType port_type;

		Port(String p_name, AutoConnect ac = AUTO_CONNECT_NONE) : name(p_name), default_value(0.f), auto_connect(ac) {}

		Port(String p_name, float p_default_value, AutoConnect ac = AUTO_CONNECT_NONE) :
				name(p_name), default_value(p_default_value), auto_connect(ac) {}
	};

	typedef Variant (*DefaultValueFactory)();

	struct Param {
		String name;
		Variant default_value;
		DefaultValueFactory default_value_func;
		Variant::Type type;
		String class_name;
		uint32_t index = -1;
		bool has_range = false;
		int min_value;
		int max_value;

		Param(String p_name, Variant::Type p_type, Variant p_default_value = Variant()) :
				name(p_name), default_value(p_default_value), type(p_type) {}

		Param(String p_name, String p_class_name, DefaultValueFactory dvf) :
				name(p_name), default_value_func(dvf), type(Variant::OBJECT), class_name(p_class_name) {}
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
		// Debug-only nodes are ignored in non-debug compilation.
		bool debug_only = false;
		// Pseudo nodes are replaced during compilation with one or multiple real nodes, they have no logic on their own
		bool is_pseudo_node = false;
		Category category;
		std::vector<Port> inputs;
		std::vector<Port> outputs;
		std::vector<Param> params;
		std::unordered_map<String, uint32_t> param_name_to_index;
		std::unordered_map<String, uint32_t> input_name_to_index;
		CompileFunc compile_func = nullptr;
		VoxelGraphRuntime::ProcessBufferFunc process_buffer_func = nullptr;
		VoxelGraphRuntime::RangeAnalysisFunc range_analysis_func = nullptr;
		// If available, name of the corresponding function to be used in expression nodes
		const char *expression_func_name = nullptr;
		// The Expression node can invoke the logic of other nodes, but it then needs a specific implementation
		ExpressionParser::FunctionCallback expression_func = nullptr;
		ShaderGenFunc shader_gen_func = nullptr;

		inline bool has_autoconnect_inputs() const {
			for (const Port &port : inputs) {
				if (port.auto_connect != AUTO_CONNECT_NONE) {
					return true;
				}
			}
			return false;
		}
	};

	VoxelGraphNodeDB();

	static const VoxelGraphNodeDB &get_singleton();
	static void create_singleton();
	static void destroy_singleton();

	static const char *get_category_name(Category category);

	int get_type_count() const {
		return _types.size();
	}
	bool is_valid_type_id(int type_id) const {
		return type_id >= 0 && type_id < static_cast<int>(_types.size());
	}
	const NodeType &get_type(uint32_t id) const {
		return _types[id];
	}
	Dictionary get_type_info_dict(uint32_t id) const;
	bool try_get_type_id_from_name(const String &name, VoxelGeneratorGraph::NodeTypeID &out_type_id) const;
	bool try_get_param_index_from_name(uint32_t type_id, const String &name, uint32_t &out_param_index) const;
	bool try_get_input_index_from_name(uint32_t type_id, const String &name, uint32_t &out_input_index) const;

	Span<const ExpressionParser::Function> get_expression_parser_functions() const {
		return to_span_const(_expression_functions);
	}

private:
	FixedArray<NodeType, VoxelGeneratorGraph::NODE_TYPE_COUNT> _types;
	std::unordered_map<String, VoxelGeneratorGraph::NodeTypeID> _type_name_to_id;
	std::vector<ExpressionParser::Function> _expression_functions;
};

} // namespace zylann::voxel

#endif // VOXEL_GRAPH_NODE_DB_H
