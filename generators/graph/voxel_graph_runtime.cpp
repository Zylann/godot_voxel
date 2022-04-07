#include "voxel_graph_runtime.h"
#include "../../util/expression_parser.h"
#include "../../util/funcs.h"
#include "../../util/macros.h"
#include "../../util/profiling.h"
#include "voxel_generator_graph.h"
#include "voxel_graph_node_db.h"

#include <unordered_set>

//#ifdef DEBUG_ENABLED
//#define VOXEL_DEBUG_GRAPH_PROG_SENTINEL uint16_t(12345) // 48, 57 (base 10)
//#endif

namespace zylann::voxel {

VoxelGraphRuntime::VoxelGraphRuntime() {
	clear();
}

VoxelGraphRuntime::~VoxelGraphRuntime() {
	clear();
}

void VoxelGraphRuntime::clear() {
	_program.clear();
}

struct ToConnect {
	std::string_view var_name;
	ProgramGraph::PortLocation dst;
};

static uint32_t expand_node(ProgramGraph &graph, const ExpressionParser::Node &ep_node, const VoxelGraphNodeDB &db,
		std::vector<ToConnect> &to_connect, std::vector<uint32_t> &expanded_node_ids,
		Span<const ExpressionParser::Function> functions);

static bool expand_input(ProgramGraph &graph, const ExpressionParser::Node &arg, ProgramGraph::Node &pg_node,
		uint32_t pg_node_input_index, const VoxelGraphNodeDB &db, std::vector<ToConnect> &to_connect,
		std::vector<uint32_t> &expanded_node_ids, Span<const ExpressionParser::Function> functions) {
	switch (arg.type) {
		case ExpressionParser::Node::NUMBER: {
			const ExpressionParser::NumberNode &arg_nn = reinterpret_cast<const ExpressionParser::NumberNode &>(arg);
			pg_node.default_inputs[pg_node_input_index] = arg_nn.value;
		} break;

		case ExpressionParser::Node::VARIABLE: {
			const ExpressionParser::VariableNode &arg_vn =
					reinterpret_cast<const ExpressionParser::VariableNode &>(arg);
			to_connect.push_back({ arg_vn.name, { pg_node.id, pg_node_input_index } });
		} break;

		case ExpressionParser::Node::OPERATOR:
		case ExpressionParser::Node::FUNCTION: {
			const uint32_t dependency_pg_node_id =
					expand_node(graph, arg, db, to_connect, expanded_node_ids, functions);
			ERR_FAIL_COND_V(dependency_pg_node_id == ProgramGraph::NULL_ID, false);
			graph.connect({ dependency_pg_node_id, 0 }, { pg_node.id, pg_node_input_index });
		} break;

		default:
			return false;
	}
	return true;
}

static ProgramGraph::Node &create_node(
		ProgramGraph &graph, const VoxelGraphNodeDB &db, VoxelGeneratorGraph::NodeTypeID node_type_id) {
	ProgramGraph::Node *node = create_node_internal(graph, node_type_id, Vector2(), ProgramGraph::NULL_ID);
	CRASH_COND(node == nullptr);
	return *node;
}

static uint32_t expand_node(ProgramGraph &graph, const ExpressionParser::Node &ep_node, const VoxelGraphNodeDB &db,
		std::vector<ToConnect> &to_connect, std::vector<uint32_t> &expanded_node_ids,
		Span<const ExpressionParser::Function> functions) {
	switch (ep_node.type) {
		case ExpressionParser::Node::NUMBER: {
			// Note, this code should only run if the whole expression is only a number.
			// Constant node inputs don't create a constant node, they just set the default value of the input.
			ProgramGraph::Node &pg_node = create_node(graph, db, VoxelGeneratorGraph::NODE_CONSTANT);
			const ExpressionParser::NumberNode &nn = reinterpret_cast<const ExpressionParser::NumberNode &>(ep_node);
			CRASH_COND(pg_node.params.size() != 1);
			pg_node.params[0] = nn.value;
			expanded_node_ids.push_back(pg_node.id);
			return pg_node.id;
		}

		case ExpressionParser::Node::VARIABLE: {
			// Note, this code should only run if the whole expression is only a variable.
			// Variable node inputs don't create a node each time, they are turned into connections in a later pass.
			// Here we need a pass-through node, so let's use `var + 0`. It's not a common case anyways.
			ProgramGraph::Node &pg_node = create_node(graph, db, VoxelGeneratorGraph::NODE_ADD);
			const ExpressionParser::VariableNode &vn =
					reinterpret_cast<const ExpressionParser::VariableNode &>(ep_node);
			to_connect.push_back({ vn.name, { pg_node.id, 0 } });
			CRASH_COND(pg_node.default_inputs.size() != 2);
			pg_node.default_inputs[1] = 0;
			expanded_node_ids.push_back(pg_node.id);
			return pg_node.id;
		}

		case ExpressionParser::Node::OPERATOR: {
			const ExpressionParser::OperatorNode &on =
					reinterpret_cast<const ExpressionParser::OperatorNode &>(ep_node);

			CRASH_COND(on.n0 == nullptr);
			CRASH_COND(on.n1 == nullptr);

			VoxelGeneratorGraph::NodeTypeID node_type_id;
			switch (on.op) {
				case ExpressionParser::OperatorNode::ADD:
					node_type_id = VoxelGeneratorGraph::NODE_ADD;
					break;
				case ExpressionParser::OperatorNode::SUBTRACT:
					node_type_id = VoxelGeneratorGraph::NODE_SUBTRACT;
					break;
				case ExpressionParser::OperatorNode::MULTIPLY:
					node_type_id = VoxelGeneratorGraph::NODE_MULTIPLY;
					break;
				case ExpressionParser::OperatorNode::DIVIDE:
					node_type_id = VoxelGeneratorGraph::NODE_DIVIDE;
					break;
				case ExpressionParser::OperatorNode::POWER:
					if (on.n1->type == ExpressionParser::Node::NUMBER) {
						// Attempt to use an optimized node if the power is constant
						const ExpressionParser::NumberNode &arg1 =
								static_cast<const ExpressionParser::NumberNode &>(*on.n1);

						const int pi = int(arg1.value);
						if (Math::is_equal_approx(arg1.value, pi) && pi >= 0) {
							// Constant positive integer
							ProgramGraph::Node &pg_node = create_node(graph, db, VoxelGeneratorGraph::NODE_POWI);
							expanded_node_ids.push_back(pg_node.id);

							CRASH_COND(pg_node.params.size() != 1);
							pg_node.params[0] = pi;

							ERR_FAIL_COND_V(!expand_input(graph, *on.n0, pg_node, 0, db, to_connect, expanded_node_ids,
													functions),
									ProgramGraph::NULL_ID);

							return pg_node.id;
						}
					}
					// Fallback on generic power function
					node_type_id = VoxelGeneratorGraph::NODE_POW;
					break;
				default:
					CRASH_NOW();
					break;
			}

			ProgramGraph::Node &pg_node = create_node(graph, db, node_type_id);
			expanded_node_ids.push_back(pg_node.id);

			ERR_FAIL_COND_V(!expand_input(graph, *on.n0, pg_node, 0, db, to_connect, expanded_node_ids, functions),
					ProgramGraph::NULL_ID);

			ERR_FAIL_COND_V(!expand_input(graph, *on.n1, pg_node, 1, db, to_connect, expanded_node_ids, functions),
					ProgramGraph::NULL_ID);

			return pg_node.id;
		}

		case ExpressionParser::Node::FUNCTION: {
			const ExpressionParser::FunctionNode &fn =
					reinterpret_cast<const ExpressionParser::FunctionNode &>(ep_node);
			const ExpressionParser::Function *f = ExpressionParser::find_function_by_id(fn.function_id, functions);
			CRASH_COND(f == nullptr);
			const unsigned int arg_count = f->argument_count;

			ProgramGraph::Node &pg_node = create_node(graph, db, VoxelGeneratorGraph::NodeTypeID(fn.function_id));
			// TODO Optimization: per-function shortcuts

			for (unsigned int arg_index = 0; arg_index < arg_count; ++arg_index) {
				const ExpressionParser::Node *arg = fn.args[arg_index].get();
				CRASH_COND(arg == nullptr);
				ERR_FAIL_COND_V(
						!expand_input(graph, *arg, pg_node, arg_index, db, to_connect, expanded_node_ids, functions),
						ProgramGraph::NULL_ID);
			}

			return pg_node.id;
		}

		default:
			return ProgramGraph::NULL_ID;
	}
}

static VoxelGraphRuntime::CompilationResult expand_expression_node(ProgramGraph &graph, uint32_t original_node_id,
		ProgramGraph::PortLocation &expanded_output_port, std::vector<uint32_t> &expanded_nodes) {
	VOXEL_PROFILE_SCOPE();
	const ProgramGraph::Node &original_node = graph.get_node(original_node_id);
	CRASH_COND(original_node.params.size() == 0);
	const String code = original_node.params[0];
	const CharString code_utf8 = code.utf8();

	Span<const ExpressionParser::Function> functions =
			VoxelGraphNodeDB::get_singleton()->get_expression_parser_functions();

	// Extract the AST, so we can convert it into graph nodes,
	// and benefit from all features of range analysis and buffer processing
	ExpressionParser::Result parse_result = ExpressionParser::parse(code_utf8.get_data(), functions);

	if (parse_result.error.id != ExpressionParser::ERROR_NONE) {
		const std::string error_message_utf8 = ExpressionParser::to_string(parse_result.error);
		VoxelGraphRuntime::CompilationResult result;
		result.success = false;
		result.node_id = original_node_id;
		result.message = String(error_message_utf8.c_str());
		return result;
	}

	if (parse_result.root == nullptr) {
		VoxelGraphRuntime::CompilationResult result;
		result.success = false;
		result.node_id = original_node_id;
		result.message = "Expression is empty";
		return result;
	}

	std::vector<ToConnect> to_connect;

	const uint32_t expanded_root_node_id = expand_node(
			graph, *parse_result.root, *VoxelGraphNodeDB::get_singleton(), to_connect, expanded_nodes, functions);
	if (expanded_root_node_id == ProgramGraph::NULL_ID) {
		VoxelGraphRuntime::CompilationResult result;
		result.success = false;
		result.node_id = original_node_id;
		result.message = "Internal error";
		return result;
	}

	expanded_output_port = { expanded_root_node_id, 0 };

	for (unsigned int i = 0; i < to_connect.size(); ++i) {
		const ToConnect tc = to_connect[i];

		unsigned int original_port_index;
		if (!original_node.find_input_port_by_name(tc.var_name, original_port_index)) {
			VoxelGraphRuntime::CompilationResult result;
			result.success = false;
			result.node_id = original_node_id;
			result.message = "Could not resolve expression variable from input ports";
			return result;
		}
		const ProgramGraph::Port &original_port = original_node.inputs[original_port_index];
		for (unsigned int j = 0; j < original_port.connections.size(); ++j) {
			const ProgramGraph::PortLocation src = original_port.connections[j];
			graph.connect(src, tc.dst);
		}
	}

	graph.remove_node(original_node_id);

	VoxelGraphRuntime::CompilationResult result;
	result.success = true;
	return result;
}

struct PortRemap {
	ProgramGraph::PortLocation original;
	ProgramGraph::PortLocation expanded;
};

struct ExpandedNodeRemap {
	uint32_t expanded_node_id;
	uint32_t original_node_id;
};

static VoxelGraphRuntime::CompilationResult expand_expression_nodes(ProgramGraph &graph,
		std::vector<PortRemap> &user_to_expanded_ports, std::vector<ExpandedNodeRemap> &expanded_to_user_node_ids) {
	VOXEL_PROFILE_SCOPE();
	// Gather expression node IDs first, as expansion could invalidate the iterator
	std::vector<uint32_t> expression_node_ids;
	graph.for_each_node([&expression_node_ids](ProgramGraph::Node &node) {
		if (node.type_id == VoxelGeneratorGraph::NODE_EXPRESSION) {
			expression_node_ids.push_back(node.id);
		}
	});

	std::vector<uint32_t> expanded_node_ids;

	for (auto it = expression_node_ids.begin(); it != expression_node_ids.end(); ++it) {
		const uint32_t node_id = *it;
		ProgramGraph::PortLocation expanded_output_port;
		expanded_node_ids.clear();
		VoxelGraphRuntime::CompilationResult result =
				expand_expression_node(graph, node_id, expanded_output_port, expanded_node_ids);
		if (!result.success) {
			return result;
		}
		user_to_expanded_ports.push_back({ { node_id, 0 }, expanded_output_port });
		for (auto it2 = expanded_node_ids.begin(); it2 != expanded_node_ids.end(); ++it2) {
			expanded_to_user_node_ids.push_back({ *it2, node_id });
		}
	}

	VoxelGraphRuntime::CompilationResult result;
	result.success = true;
	return result;
}

VoxelGraphRuntime::CompilationResult VoxelGraphRuntime::compile(const ProgramGraph &p_graph, bool debug) {
	VOXEL_PROFILE_SCOPE();

	ProgramGraph graph;
	graph.copy_from(p_graph, false);
	// TODO Store a remapping to allow debugging with the expanded graph
	std::vector<PortRemap> user_to_expanded_ports;
	std::vector<ExpandedNodeRemap> expanded_to_user_node_ids;
	VoxelGraphRuntime::CompilationResult expand_result =
			expand_expression_nodes(graph, user_to_expanded_ports, expanded_to_user_node_ids);
	if (!expand_result.success) {
		return expand_result;
	}

	VoxelGraphRuntime::CompilationResult result = _compile(graph, debug);
	if (!result.success) {
		clear();
	}

	for (PortRemap r : user_to_expanded_ports) {
		_program.user_port_to_expanded_port.insert({ r.original, r.expanded });
	}
	for (ExpandedNodeRemap r : expanded_to_user_node_ids) {
		_program.expanded_node_id_to_user_node_id.insert({ r.expanded_node_id, r.original_node_id });
	}

	return result;
}

VoxelGraphRuntime::CompilationResult VoxelGraphRuntime::_compile(const ProgramGraph &graph, bool debug) {
	VOXEL_PROFILE_SCOPE();
	clear();

	std::vector<uint32_t> order;
	std::vector<uint32_t> terminal_nodes;
	std::unordered_map<uint32_t, uint32_t> node_id_to_dependency_graph;

	// Not using the generic `get_terminal_nodes` function because our terminal nodes do have outputs
	graph.for_each_node_const([&terminal_nodes](const ProgramGraph::Node &node) {
		const VoxelGraphNodeDB::NodeType &type = VoxelGraphNodeDB::get_singleton()->get_type(node.type_id);
		if (type.category == VoxelGraphNodeDB::CATEGORY_OUTPUT) {
			terminal_nodes.push_back(node.id);
		}
	});

	if (!debug) {
		// Exclude debug nodes
		unordered_remove_if(terminal_nodes, [&graph](uint32_t node_id) {
			const ProgramGraph::Node &node = graph.get_node(node_id);
			const VoxelGraphNodeDB::NodeType &type = VoxelGraphNodeDB::get_singleton()->get_type(node.type_id);
			return type.debug_only;
		});
	}

	graph.find_dependencies(terminal_nodes, order);

	uint32_t xzy_start_index = 0;

	// Optimize parts of the graph that only depend on X and Z,
	// so they can be moved in the outer loop when blocks are generated, running less times.
	// Moves them all at the beginning.
	{
		std::vector<uint32_t> immediate_deps;
		std::unordered_set<uint32_t> nodes_depending_on_y;
		std::vector<uint32_t> order_xz;
		std::vector<uint32_t> order_xzy;

		for (size_t i = 0; i < order.size(); ++i) {
			const uint32_t node_id = order[i];
			const ProgramGraph::Node &node = graph.get_node(node_id);

			bool depends_on_y = false;

			if (node.type_id == VoxelGeneratorGraph::NODE_INPUT_Y) {
				nodes_depending_on_y.insert(node_id);
				depends_on_y = true;
			}

			if (!depends_on_y) {
				immediate_deps.clear();
				graph.find_immediate_dependencies(node_id, immediate_deps);

				for (size_t j = 0; j < immediate_deps.size(); ++j) {
					const uint32_t dep_node_id = immediate_deps[j];

					if (nodes_depending_on_y.find(dep_node_id) != nodes_depending_on_y.end()) {
						depends_on_y = true;
						nodes_depending_on_y.insert(node_id);
						break;
					}
				}
			}

			if (depends_on_y) {
				order_xzy.push_back(node_id);
			} else {
				order_xz.push_back(node_id);
			}
		}

		xzy_start_index = order_xz.size();

		//#ifdef DEBUG_ENABLED
		//		const uint32_t order_xz_raw_size = order_xz.size();
		//		const uint32_t *order_xz_raw = order_xz.data();
		//		const uint32_t order_xzy_raw_size = order_xzy.size();
		//		const uint32_t *order_xzy_raw = order_xzy.data();
		//#endif

		size_t i = 0;
		for (size_t j = 0; j < order_xz.size(); ++j) {
			order[i++] = order_xz[j];
		}
		for (size_t j = 0; j < order_xzy.size(); ++j) {
			order[i++] = order_xzy[j];
		}
	}

	//#ifdef DEBUG_ENABLED
	//	const uint32_t order_raw_size = order.size();
	//	const uint32_t *order_raw = order.data();
	//#endif

	struct MemoryHelper {
		std::vector<BufferSpec> &buffer_specs;
		unsigned int next_address = 0;

		uint16_t add_binding() {
			const unsigned int a = next_address;
			++next_address;
			BufferSpec bs;
			bs.address = a;
			bs.is_binding = true;
			bs.is_constant = false;
			bs.users_count = 0;
			buffer_specs.push_back(bs);
			return a;
		}

		uint16_t add_var() {
			const unsigned int a = next_address;
			++next_address;
			BufferSpec bs;
			bs.address = a;
			bs.is_binding = false;
			bs.is_constant = false;
			bs.users_count = 0;
			buffer_specs.push_back(bs);
			return a;
		}

		uint16_t add_constant(float v) {
			const unsigned int a = next_address;
			++next_address;
			BufferSpec bs;
			bs.address = a;
			bs.constant_value = v;
			bs.is_binding = false;
			bs.is_constant = true;
			bs.users_count = 0;
			buffer_specs.push_back(bs);
			return a;
		}
	};

	MemoryHelper mem{ _program.buffer_specs };

	// Main inputs X, Y, Z
	_program.x_input_address = mem.add_binding();
	_program.y_input_address = mem.add_binding();
	_program.z_input_address = mem.add_binding();

	std::vector<uint16_t> &operations = _program.operations;
	const VoxelGraphNodeDB &type_db = *VoxelGraphNodeDB::get_singleton();

	// Run through each node in order, and turn them into program instructions
	for (size_t order_index = 0; order_index < order.size(); ++order_index) {
		const uint32_t node_id = order[order_index];
		const ProgramGraph::Node &node = graph.get_node(node_id);
		const VoxelGraphNodeDB::NodeType &type = type_db.get_type(node.type_id);

		CRASH_COND(node.inputs.size() != type.inputs.size());
		CRASH_COND(node.outputs.size() != type.outputs.size());

		if (order_index == xzy_start_index) {
			_program.xzy_start_op_address = operations.size();
		}

		const unsigned int dg_node_index = _program.dependency_graph.nodes.size();
		_program.dependency_graph.nodes.push_back(DependencyGraph::Node());
		DependencyGraph::Node &dg_node = _program.dependency_graph.nodes.back();
		dg_node.is_input = false;
		dg_node.op_address = operations.size();
		dg_node.first_dependency = _program.dependency_graph.dependencies.size();
		dg_node.end_dependency = dg_node.first_dependency;
		dg_node.debug_node_id = node_id;
		node_id_to_dependency_graph.insert(std::make_pair(node_id, dg_node_index));

		// We still hardcode some of the nodes. Maybe we can abstract them too one day.
		switch (node.type_id) {
			case VoxelGeneratorGraph::NODE_CONSTANT: {
				CRASH_COND(type.outputs.size() != 1);
				CRASH_COND(type.params.size() != 1);
				const uint16_t a = mem.add_constant(node.params[0].operator float());
				_program.output_port_addresses[ProgramGraph::PortLocation{ node_id, 0 }] = a;
				// Technically not an input or an output, but is a dependency regardless so treat it like an input
				dg_node.is_input = true;
				continue;
			}

			// Input nodes can appear multiple times in the graph, for convenience.
			// Multiple instances of the same node will refer to the same data.
			case VoxelGeneratorGraph::NODE_INPUT_X:
				_program.output_port_addresses[ProgramGraph::PortLocation{ node_id, 0 }] = _program.x_input_address;
				dg_node.is_input = true;
				continue;

			case VoxelGeneratorGraph::NODE_INPUT_Y:
				_program.output_port_addresses[ProgramGraph::PortLocation{ node_id, 0 }] = _program.y_input_address;
				dg_node.is_input = true;
				continue;

			case VoxelGeneratorGraph::NODE_INPUT_Z:
				_program.output_port_addresses[ProgramGraph::PortLocation{ node_id, 0 }] = _program.z_input_address;
				dg_node.is_input = true;
				continue;

			case VoxelGeneratorGraph::NODE_SDF_PREVIEW:
				continue;
		}

		// Add actual operation

		CRASH_COND(node.type_id > 0xff);

		if (order_index == xzy_start_index) {
			_program.default_execution_map.xzy_start_index = _program.default_execution_map.operation_adresses.size();
		}
		_program.default_execution_map.operation_adresses.push_back(operations.size());

		operations.push_back(node.type_id);

		// Inputs and outputs use a convention so we can have generic code for them.
		// Parameters are more specific, and may be affected by alignment so better just do them by hand

		// Add inputs
		for (size_t j = 0; j < type.inputs.size(); ++j) {
			uint16_t a;

			if (node.inputs[j].connections.size() == 0) {
				// No input, default it
				CRASH_COND(j >= node.default_inputs.size());
				float defval = node.default_inputs[j];
				a = mem.add_constant(defval);

			} else {
				ProgramGraph::PortLocation src_port = node.inputs[j].connections[0];
				const uint16_t *aptr = _program.output_port_addresses.getptr(src_port);
				// Previous node ports must have been registered
				CRASH_COND(aptr == nullptr);
				a = *aptr;

				// Register dependency
				auto it = node_id_to_dependency_graph.find(src_port.node_id);
				CRASH_COND(it == node_id_to_dependency_graph.end());
				CRASH_COND(it->second >= _program.dependency_graph.nodes.size());
				_program.dependency_graph.dependencies.push_back(it->second);
				++dg_node.end_dependency;
			}

			operations.push_back(a);

			BufferSpec &bs = _program.buffer_specs[a];
			++bs.users_count;
		}

		// Add outputs
		for (size_t j = 0; j < type.outputs.size(); ++j) {
			const uint16_t a = mem.add_var();

			// This will be used by next nodes
			const ProgramGraph::PortLocation op{ node_id, static_cast<uint32_t>(j) };
			_program.output_port_addresses[op] = a;

			operations.push_back(a);
		}

		// Add space for params size, default is no params so size is 0
		size_t params_size_index = operations.size();
		operations.push_back(0);

		// Get params, copy resources when used, and hold a reference to them
		std::vector<Variant> params_copy;
		params_copy.resize(node.params.size());
		for (size_t i = 0; i < node.params.size(); ++i) {
			Variant v = node.params[i];

			if (v.get_type() == Variant::OBJECT) {
				Ref<Resource> res = v;

				if (res.is_null()) {
					// duplicate() is only available in Resource,
					// so we have to limit to this instead of Reference or Object
					CompilationResult result;
					result.success = false;
					result.message = ZN_TTR("A parameter is an object but does not inherit Resource");
					result.node_id = node_id;
					return result;
				}

				res = res->duplicate();

				_program.ref_resources.push_back(res);
				v = res;
			}

			params_copy[i] = v;
		}

		if (type.compile_func != nullptr) {
			CompileContext ctx(/**node,*/ operations, _program.heap_resources, params_copy);
			type.compile_func(ctx);
			if (ctx.has_error()) {
				CompilationResult result;
				result.success = false;
				result.message = ctx.get_error_message();
				result.node_id = node_id;
				return result;
			}
			const size_t params_size = ctx.get_params_size_in_words();
			CRASH_COND(params_size > std::numeric_limits<uint16_t>::max());
			operations[params_size_index] = params_size;
		}

		if (type.category == VoxelGraphNodeDB::CATEGORY_OUTPUT) {
			CRASH_COND(node.outputs.size() != 1);

			if (_program.outputs_count == _program.outputs.size()) {
				CompilationResult result;
				result.success = false;
				result.message = ZN_TTR("Maximum number of outputs has been reached");
				result.node_id = node_id;
				return result;
			}

			{
				const uint16_t *aptr = _program.output_port_addresses.getptr(ProgramGraph::PortLocation{ node_id, 0 });
				// Previous node ports must have been registered
				CRASH_COND(aptr == nullptr);
				OutputInfo &output_info = _program.outputs[_program.outputs_count];
				output_info.buffer_address = *aptr;
				output_info.dependency_graph_node_index = dg_node_index;
				output_info.node_id = node_id;
				++_program.outputs_count;
			}

			// Add fake user for output ports so they can pass the local users check in optimizations
			for (unsigned int j = 0; j < type.outputs.size(); ++j) {
				const ProgramGraph::PortLocation loc{ node_id, j };
				const uint16_t *aptr = _program.output_port_addresses.getptr(loc);
				CRASH_COND(aptr == nullptr);
				BufferSpec &bs = _program.buffer_specs[*aptr];
				// Not expecting existing users on that port
				ERR_FAIL_COND_V(bs.users_count != 0, CompilationResult());
				++bs.users_count;
			}
		}

#ifdef VOXEL_DEBUG_GRAPH_PROG_SENTINEL
		// Append a special value after each operation
		append(operations, VOXEL_DEBUG_GRAPH_PROG_SENTINEL);
#endif
	}

	_program.buffer_count = mem.next_address;

	ZN_PRINT_VERBOSE(String("Compiled voxel graph. Program size: {0}b, buffers: {1}")
							 .format(varray(ZN_SIZE_T_TO_VARIANT(_program.operations.size() * sizeof(uint16_t)),
									 ZN_SIZE_T_TO_VARIANT(_program.buffer_count))));

	CompilationResult result;
	result.success = true;
	return result;
}

static Span<const uint16_t> get_outputs_from_op_address(Span<const uint16_t> operations, uint16_t op_address) {
	const uint16_t opid = operations[op_address];
	const VoxelGraphNodeDB::NodeType &node_type = VoxelGraphNodeDB::get_singleton()->get_type(opid);

	const uint32_t inputs_count = node_type.inputs.size();
	const uint32_t outputs_count = node_type.outputs.size();

	// The +1 is for `opid`
	return operations.sub(op_address + 1 + inputs_count, outputs_count);
}

bool VoxelGraphRuntime::is_operation_constant(const State &state, uint16_t op_address) const {
	Span<const uint16_t> outputs = get_outputs_from_op_address(to_span_const(_program.operations), op_address);

	for (unsigned int i = 0; i < outputs.size(); ++i) {
		const uint16_t output_address = outputs[i];
		const Buffer &buffer = state.get_buffer(output_address);
		if (!(buffer.is_constant || state.get_range(output_address).is_single_value() ||
					buffer.local_users_count == 0)) {
			// At least one of the outputs cannot be predicted in the current area
			return false;
		}
	}

	return true;
}

void VoxelGraphRuntime::generate_optimized_execution_map(
		const State &state, ExecutionMap &execution_map, bool debug) const {
	FixedArray<unsigned int, MAX_OUTPUTS> all_outputs;
	for (unsigned int i = 0; i < _program.outputs_count; ++i) {
		all_outputs[i] = i;
	}
	generate_optimized_execution_map(state, execution_map, to_span_const(all_outputs, _program.outputs_count), debug);
}

// Generates a list of adresses for the operations to execute,
// skipping those that are deemed constant by the last range analysis.
// If a non-constant operation only contributes to a constant one, it will also be skipped.
// This has the effect of optimizing locally at runtime without relying on explicit conditionals.
// It can be useful for biomes, where some branches become constant when not used in the final blending.
void VoxelGraphRuntime::generate_optimized_execution_map(
		const State &state, ExecutionMap &execution_map, Span<const unsigned int> required_outputs, bool debug) const {
	VOXEL_PROFILE_SCOPE();

	// Range analysis results must have been computed
	ERR_FAIL_COND(state.ranges.size() == 0);

	const Program &program = _program;
	const DependencyGraph &graph = program.dependency_graph;

	execution_map.clear();

	// if (program.default_execution_map.size() == 0) {
	// 	// Can't reduce more than this
	// 	return;
	// }

	// This function will run a lot of times so better re-use the same vector
	static thread_local std::vector<uint16_t> to_process;
	to_process.clear();

	for (unsigned int i = 0; i < required_outputs.size(); ++i) {
		const unsigned int output_index = required_outputs[i];
		const unsigned int dg_index = program.outputs[output_index].dependency_graph_node_index;
		to_process.push_back(dg_index);
	}

	enum ProcessResult { NOT_PROCESSED, SKIPPABLE, REQUIRED };

	static thread_local std::vector<ProcessResult> results;
	results.clear();
	results.resize(graph.nodes.size(), NOT_PROCESSED);

	while (to_process.size() != 0) {
		const uint32_t node_index = to_process.back();
		const unsigned int to_process_previous_size = to_process.size();

		// Check needed because Godot never compiles with `_DEBUG`...
#ifdef DEBUG_ENABLED
		CRASH_COND(node_index >= graph.nodes.size());
#endif
		const DependencyGraph::Node &node = graph.nodes[node_index];

		// Ignore inputs because they are not present in the operations list
		if (!node.is_input && is_operation_constant(state, node.op_address)) {
			// Skip this operation for now.
			// If no other dependency reaches it, it will be effectively skipped in the result.
			to_process.pop_back();
			results[node_index] = SKIPPABLE;
			continue;
		}

		for (uint32_t i = node.first_dependency; i < node.end_dependency; ++i) {
			const uint32_t dep_node_index = graph.dependencies[i];
			if (results[dep_node_index] != NOT_PROCESSED) {
				// Already processed
				continue;
			}
			to_process.push_back(dep_node_index);
		}

		if (to_process_previous_size == to_process.size()) {
			to_process.pop_back();
			results[node_index] = REQUIRED;
		}
	}

	if (debug) {
		std::vector<uint32_t> &debug_nodes = execution_map.debug_nodes;

		for (unsigned int node_index = 0; node_index < graph.nodes.size(); ++node_index) {
			const ProcessResult res = results[node_index];
			const DependencyGraph::Node &node = graph.nodes[node_index];

			if (res == REQUIRED) {
				uint32_t debug_node_id = node.debug_node_id;
				auto it = _program.expanded_node_id_to_user_node_id.find(debug_node_id);

				if (it != _program.expanded_node_id_to_user_node_id.end()) {
					debug_node_id = it->second;
					if (std::find(debug_nodes.begin(), debug_nodes.end(), debug_node_id) != debug_nodes.end()) {
						// Ignore duplicates. Some nodes can have been expanded into multiple ones.
						continue;
					}
				}

				debug_nodes.push_back(node.debug_node_id);
			}
		}
	}

	Span<const uint16_t> operations(program.operations.data(), 0, program.operations.size());
	bool xzy_start_not_assigned = true;

	// Now we have to fill buffers with the local constants we may have found.
	// We iterate nodes primarily because we have to preserve a certain order relative to outer loop optimization.
	for (unsigned int node_index = 0; node_index < graph.nodes.size(); ++node_index) {
		const ProcessResult res = results[node_index];
		const DependencyGraph::Node &node = graph.nodes[node_index];

		if (node.is_input) {
			continue;
		}

		switch (res) {
			case NOT_PROCESSED:
				continue;

			case SKIPPABLE: {
				const Span<const uint16_t> outputs = get_outputs_from_op_address(operations, node.op_address);

				for (unsigned int output_index = 0; output_index < outputs.size(); ++output_index) {
					const uint16_t output_address = outputs[output_index];
					const Buffer &buffer = state.get_buffer(output_address);

					if (buffer.is_constant) {
						// Already assigned at prepare-time
						continue;
					}

					CRASH_COND(buffer.is_binding);

					// The node is considered skippable, which means its outputs are either locally constant or unused.
					// Unused buffers can be left as-is, but local constants must be filled in.
					if (buffer.local_users_count > 0) {
						const math::Interval range = state.ranges[output_address];
						// If this interval is not a single value then the node should not have been skippable
						CRASH_COND(!range.is_single_value());
						const float v = range.min;
						for (unsigned int j = 0; j < buffer.size; ++j) {
							buffer.data[j] = v;
						}
					}
				}
			} break;

			case REQUIRED:
				if (xzy_start_not_assigned && node.op_address >= program.xzy_start_op_address) {
					// This should be correct as long as the list of nodes in the graph follows the same re-ordered
					// optimization done in `compile()` such that all nodes not depending on Y come first
					execution_map.xzy_start_index = execution_map.operation_adresses.size();
					xzy_start_not_assigned = false;
				}
				execution_map.operation_adresses.push_back(node.op_address);
				break;

			default:
				CRASH_NOW();
				break;
		}
	}
}

void VoxelGraphRuntime::generate_single(State &state, Vector3f position_f, const ExecutionMap *execution_map) const {
	generate_set(state, Span<float>(&position_f.x, 1), Span<float>(&position_f.y, 1), Span<float>(&position_f.z, 1),
			false, execution_map);
}

void VoxelGraphRuntime::prepare_state(State &state, unsigned int buffer_size) const {
	const unsigned int old_buffer_count = state.buffers.size();
	if (_program.buffer_count > state.buffers.size()) {
		state.buffers.resize(_program.buffer_count);
	}

	// Note: this must be after we resize the vector
	Span<Buffer> buffers(state.buffers, 0, state.buffers.size());
	state.buffer_size = buffer_size;

	for (auto it = _program.buffer_specs.cbegin(); it != _program.buffer_specs.cend(); ++it) {
		const BufferSpec &buffer_spec = *it;
		Buffer &buffer = buffers[buffer_spec.address];

		if (buffer_spec.is_binding) {
			if (buffer.is_binding) {
				// Forgot to unbind?
				CRASH_COND(buffer.data != nullptr);
			} else if (buffer.data != nullptr) {
				// Deallocate this buffer if it wasnt a binding and contained something
				memfree(buffer.data);
			}
		}

		buffer.is_binding = buffer_spec.is_binding;
	}

	// Allocate more buffers if needed
	if (old_buffer_count < state.buffers.size()) {
		for (size_t buffer_index = old_buffer_count; buffer_index < buffers.size(); ++buffer_index) {
			Buffer &buffer = buffers[buffer_index];
			// TODO Put all bindings at the beginning. This would avoid the branch.
			if (buffer.is_binding) {
				// These are supposed to be setup already
				continue;
			}
			// We don't expect previous stuff in those buffers since we just created their slots
			CRASH_COND(buffer.data != nullptr);
			// TODO Use pool?
			// New buffers get an up-to-date size, but must also comply with common capacity
			const unsigned int bs = math::max(state.buffer_capacity, buffer_size);
			buffer.data = reinterpret_cast<float *>(memalloc(bs * sizeof(float)));
			buffer.capacity = bs;
		}
	}

	// Make old buffers larger if needed
	if (state.buffer_capacity < buffer_size) {
		for (size_t buffer_index = 0; buffer_index < old_buffer_count; ++buffer_index) {
			Buffer &buffer = buffers[buffer_index];
			if (buffer.is_binding) {
				continue;
			}
			if (buffer.data == nullptr) {
				buffer.data = reinterpret_cast<float *>(memalloc(buffer_size * sizeof(float)));
			} else {
				buffer.data = reinterpret_cast<float *>(memrealloc(buffer.data, buffer_size * sizeof(float)));
			}
			buffer.capacity = buffer_size;
		}
		state.buffer_capacity = buffer_size;
	}
	for (auto it = state.buffers.begin(); it != state.buffers.end(); ++it) {
		Buffer &buffer = *it;
		buffer.size = buffer_size;
		buffer.is_constant = false;
	}

	state.ranges.resize(_program.buffer_count);

	// Always reset constants because we don't know if we'll run the same program as before...
	for (auto it = _program.buffer_specs.cbegin(); it != _program.buffer_specs.cend(); ++it) {
		const BufferSpec &bs = *it;
		Buffer &buffer = buffers[bs.address];
		if (bs.is_constant) {
			buffer.is_constant = true;
			buffer.constant_value = bs.constant_value;
			CRASH_COND(buffer.size > buffer.capacity);
			for (unsigned int j = 0; j < buffer_size; ++j) {
				buffer.data[j] = bs.constant_value;
			}
			CRASH_COND(bs.address >= state.ranges.size());
			state.ranges[bs.address] = math::Interval::from_single_value(bs.constant_value);
		}
	}

	// Puts sentinel values in buffers to detect non-initialized ones
	// #ifdef DEBUG_ENABLED
	// 	for (unsigned int i = 0; i < state.buffers.size(); ++i) {
	// 		Buffer &buffer = state.buffers[i];
	// 		if (!buffer.is_constant && !buffer.is_binding) {
	// 			CRASH_COND(buffer.data == nullptr);
	// 			for (unsigned int j = 0; j < buffer.size; ++j) {
	// 				buffer.data[j] = -969696.f;
	// 			}
	// 		}
	// 	}
	// #endif

	/*if (use_range_analysis) {
		// TODO To be really worth it, we may need a runtime graph traversal pass,
		// where we build an execution map of nodes that are worthy 🔨

		const float ra_min = _memory[i];
		const float ra_max = _memory[i + _memory.size() / 2];

		buffer.is_constant = (ra_min == ra_max);
		if (buffer.is_constant) {
			buffer.constant_value = ra_min;
		}
	}*/
}

static inline Span<const uint8_t> read_params(Span<const uint16_t> operations, unsigned int &pc) {
	const uint16_t params_size_in_words = operations[pc];
	++pc;
	Span<const uint8_t> params;
	if (params_size_in_words > 0) {
		const size_t params_offset_in_words = operations[pc];
		// Seek to aligned position where params start
		pc += params_offset_in_words;
		params = operations.sub(pc, params_size_in_words).reinterpret_cast_to<const uint8_t>();
		pc += params_size_in_words;
	}
	return params;
}

void VoxelGraphRuntime::generate_set(State &state, Span<float> in_x, Span<float> in_y, Span<float> in_z, bool skip_xz,
		const ExecutionMap *execution_map) const {
	// I don't like putting private helper functions in headers.
	struct L {
		static inline void bind_buffer(Span<Buffer> buffers, int a, Span<float> d) {
			Buffer &buffer = buffers[a];
			CRASH_COND(!buffer.is_binding);
			buffer.data = d.data();
			buffer.size = d.size();
		}

		static inline void unbind_buffer(Span<Buffer> buffers, int a) {
			Buffer &buffer = buffers[a];
			CRASH_COND(!buffer.is_binding);
			buffer.data = nullptr;
		}
	};

	VOXEL_PROFILE_SCOPE();

#ifdef DEBUG_ENABLED
	// Each array must have the same size
	CRASH_COND(!(in_x.size() == in_y.size() && in_y.size() == in_z.size()));
#endif

	const unsigned int buffer_size = in_x.size();

#ifdef TOOLS_ENABLED
	ERR_FAIL_COND(state.buffers.size() < _program.buffer_count);
	ERR_FAIL_COND(state.buffers.size() == 0);
	ERR_FAIL_COND(state.buffer_size < buffer_size);
	ERR_FAIL_COND(state.buffers[0].size < buffer_size);
#ifdef DEBUG_ENABLED
	for (size_t i = 0; i < state.buffers.size(); ++i) {
		const Buffer &b = state.buffers[i];
		CRASH_COND(b.size < buffer_size);
		CRASH_COND(b.size > state.buffer_capacity);
		CRASH_COND(b.size != state.buffer_size);
		if (!b.is_binding) {
			CRASH_COND(b.size > b.capacity);
		}
	}
#endif
#endif

	Span<Buffer> buffers(state.buffers, 0, state.buffers.size());

	// Bind inputs
	if (_program.x_input_address != -1) {
		L::bind_buffer(buffers, _program.x_input_address, in_x);
	}
	if (_program.y_input_address != -1) {
		L::bind_buffer(buffers, _program.y_input_address, in_y);
	}
	if (_program.z_input_address != -1) {
		L::bind_buffer(buffers, _program.z_input_address, in_z);
	}

	const Span<const uint16_t> operations(_program.operations.data(), 0, _program.operations.size());

	Span<const uint16_t> op_adresses = execution_map != nullptr
			? to_span_const(execution_map->operation_adresses)
			: to_span_const(_program.default_execution_map.operation_adresses);
	if (skip_xz && op_adresses.size() > 0) {
		const unsigned int offset = execution_map != nullptr ? execution_map->xzy_start_index
															 : _program.default_execution_map.xzy_start_index;
		op_adresses = op_adresses.sub(offset);
	}

	for (unsigned int execution_map_index = 0; execution_map_index < op_adresses.size(); ++execution_map_index) {
		unsigned int pc = op_adresses[execution_map_index];

		const uint16_t opid = operations[pc++];
		const VoxelGraphNodeDB::NodeType &node_type = VoxelGraphNodeDB::get_singleton()->get_type(opid);

		const uint32_t inputs_count = node_type.inputs.size();
		const uint32_t outputs_count = node_type.outputs.size();

		const Span<const uint16_t> inputs = operations.sub(pc, inputs_count);
		pc += inputs_count;
		const Span<const uint16_t> outputs = operations.sub(pc, outputs_count);
		pc += outputs_count;

		Span<const uint8_t> params = read_params(operations, pc);

		// TODO Buffers will stay bound if this error occurs!
		ERR_FAIL_COND(node_type.process_buffer_func == nullptr);
		ProcessBufferContext ctx(inputs, outputs, params, buffers, execution_map != nullptr);
		node_type.process_buffer_func(ctx);
	}

	// Unbind buffers
	if (_program.x_input_address != -1) {
		L::unbind_buffer(buffers, _program.x_input_address);
	}
	if (_program.y_input_address != -1) {
		L::unbind_buffer(buffers, _program.y_input_address);
	}
	if (_program.z_input_address != -1) {
		L::unbind_buffer(buffers, _program.z_input_address);
	}
}

// TODO Accept float bounds
void VoxelGraphRuntime::analyze_range(State &state, Vector3i min_pos, Vector3i max_pos) const {
	VOXEL_PROFILE_SCOPE();

#ifdef TOOLS_ENABLED
	ERR_FAIL_COND(state.ranges.size() != _program.buffer_count);
#endif

	Span<math::Interval> ranges(state.ranges, 0, state.ranges.size());
	Span<Buffer> buffers(state.buffers, 0, state.buffers.size());

	// Reset users count, as they might be decreased during the analysis
	for (auto it = _program.buffer_specs.cbegin(); it != _program.buffer_specs.cend(); ++it) {
		const BufferSpec &bs = *it;
		Buffer &b = buffers[bs.address];
		b.local_users_count = bs.users_count;
	}

	ranges[_program.x_input_address] = math::Interval(min_pos.x, max_pos.x);
	ranges[_program.y_input_address] = math::Interval(min_pos.y, max_pos.y);
	ranges[_program.z_input_address] = math::Interval(min_pos.z, max_pos.z);

	const Span<const uint16_t> operations(_program.operations.data(), 0, _program.operations.size());

	// Here operations must all be analyzed, because we do this as a broad-phase.
	// Only narrow-phase may skip some operations eventually.
	uint32_t pc = 0;
	while (pc < operations.size()) {
		const uint16_t opid = operations[pc++];
		const VoxelGraphNodeDB::NodeType &node_type = VoxelGraphNodeDB::get_singleton()->get_type(opid);

		const uint32_t inputs_count = node_type.inputs.size();
		const uint32_t outputs_count = node_type.outputs.size();

		const Span<const uint16_t> inputs = operations.sub(pc, inputs_count);
		pc += inputs_count;
		const Span<const uint16_t> outputs = operations.sub(pc, outputs_count);
		pc += outputs_count;

		Span<const uint8_t> params = read_params(operations, pc);

		ERR_FAIL_COND(node_type.range_analysis_func == nullptr);
		RangeAnalysisContext ctx(inputs, outputs, params, ranges, buffers);
		node_type.range_analysis_func(ctx);

#ifdef VOXEL_DEBUG_GRAPH_PROG_SENTINEL
		// If this fails, the program is ill-formed
		CRASH_COND(read<uint16_t>(_program, pc) != VOXEL_DEBUG_GRAPH_PROG_SENTINEL);
#endif
	}
}

bool VoxelGraphRuntime::try_get_output_port_address(ProgramGraph::PortLocation port, uint16_t &out_address) const {
	auto it = _program.user_port_to_expanded_port.find(port);
	if (it != _program.user_port_to_expanded_port.end()) {
		port = it->second;
	}
	const uint16_t *aptr = _program.output_port_addresses.getptr(port);
	if (aptr == nullptr) {
		// This port did not take part of the compiled result
		return false;
	}
	out_address = *aptr;
	return true;
}

} // namespace zylann::voxel
