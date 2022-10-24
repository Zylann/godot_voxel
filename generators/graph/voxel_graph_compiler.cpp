#include "voxel_graph_compiler.h"
#include "../../util/container_funcs.h"
#include "../../util/expression_parser.h"
#include "../../util/macros.h"
#include "../../util/profiling.h"
#include "../../util/string_funcs.h"
#include "voxel_graph_node_db.h"
#include <limits>
#include <unordered_set>

namespace zylann::voxel {

// Updates remaps for replacing one node with one other, where old and new nodes have the same number of outputs.
void add_remap(GraphRemappingInfo &remaps, uint32_t old_node_id, uint32_t new_node_id, unsigned int output_count) {
	bool existing_remap = false;
	// Modify existing entries in case the old node was the result of a previous processing of the graph.
	for (PortRemap &remap : remaps.user_to_expanded_ports) {
		if (remap.expanded.node_id == old_node_id) {
			remap.expanded.node_id = new_node_id;
			existing_remap = true;
		}
	}
	for (ExpandedNodeRemap &remap : remaps.expanded_to_user_node_ids) {
		if (remap.expanded_node_id == old_node_id) {
			remap.expanded_node_id = new_node_id;
		}
	}
	if (!existing_remap) {
		// The old node wasn't the result of a previous processing of the graph.
		// So we may add new entries.
		for (uint32_t output_index = 0; output_index < output_count; ++output_index) {
			PortRemap port_remap;
			port_remap.original = ProgramGraph::PortLocation{ old_node_id, output_index };
			port_remap.expanded = ProgramGraph::PortLocation{ new_node_id, output_index };
			remaps.user_to_expanded_ports.push_back(port_remap);

			remaps.expanded_to_user_node_ids.push_back(ExpandedNodeRemap{ new_node_id, old_node_id });
		}
	}
}

// Updates remaps for replacing one node with multiple nodes. Outputs of the original node can become outputs of
// different expanded nodes.
void add_remap(GraphRemappingInfo &remaps, uint32_t old_node_id, Span<const uint32_t> new_node_ids,
		Span<const ProgramGraph::PortLocation> output_locations) {
	// Add remap for the output ports
	{
		bool found = false;
		for (PortRemap &pr : remaps.user_to_expanded_ports) {
			if (pr.expanded.node_id == old_node_id) {
				// The old node is the result of a previous expansion
				pr.expanded = output_locations[pr.expanded.port_index];
				found = true;
			}
		}
		if (!found) {
			for (unsigned int output_index = 0; output_index < output_locations.size(); ++output_index) {
				PortRemap pr;
				pr.original.node_id = old_node_id;
				pr.original.port_index = output_index;
				pr.expanded = output_locations[output_index];
				remaps.user_to_expanded_ports.push_back(pr);
			}
		}
	}
	// Add remap for the nodes
	{
		uint32_t original_node_id = old_node_id;
		for (const ExpandedNodeRemap &nr : remaps.expanded_to_user_node_ids) {
			if (nr.expanded_node_id == old_node_id) {
				// The old node was itself an expanded node, trace it back to the original
				original_node_id = nr.original_node_id;
				break;
			}
		}
		for (const uint32_t expanded_node_id : new_node_ids) {
			remaps.expanded_to_user_node_ids.push_back({ expanded_node_id, original_node_id });
		}
	}
}

inline void add_remap(GraphRemappingInfo &remaps, uint32_t old_node_id, Span<const uint32_t> new_node_ids,
		const ProgramGraph::PortLocation output_location) {
	add_remap(remaps, old_node_id, new_node_ids, Span<const ProgramGraph::PortLocation>(&output_location, 1));
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
			ZN_ASSERT(pg_node_input_index < pg_node.default_inputs.size());
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
			ZN_ASSERT_RETURN_V(dependency_pg_node_id != ProgramGraph::NULL_ID, false);
			graph.connect({ dependency_pg_node_id, 0 }, { pg_node.id, pg_node_input_index });
		} break;

		default:
			return false;
	}
	return true;
}

static ProgramGraph::Node &create_node(
		ProgramGraph &graph, const VoxelGraphNodeDB &db, VoxelGraphFunction::NodeTypeID node_type_id) {
	// Not creating default sub-resources here, there are no use cases where we use such nodes.
	ProgramGraph::Node *node = create_node_internal(graph, node_type_id, Vector2(), ProgramGraph::NULL_ID, false);
	ZN_ASSERT(node != nullptr);
	return *node;
}

static uint32_t expand_node(ProgramGraph &graph, const ExpressionParser::Node &ep_node, const VoxelGraphNodeDB &db,
		std::vector<ToConnect> &to_connect, std::vector<uint32_t> &expanded_node_ids,
		Span<const ExpressionParser::Function> functions) {
	switch (ep_node.type) {
		case ExpressionParser::Node::NUMBER: {
			// Note, this code should only run if the whole expression is only a number.
			// Constant node inputs don't create a constant node, they just set the default value of the input.
			ProgramGraph::Node &pg_node = create_node(graph, db, VoxelGraphFunction::NODE_CONSTANT);
			const ExpressionParser::NumberNode &nn = reinterpret_cast<const ExpressionParser::NumberNode &>(ep_node);
			ZN_ASSERT(pg_node.params.size() == 1);
			pg_node.params[0] = nn.value;
			expanded_node_ids.push_back(pg_node.id);
			return pg_node.id;
		}

		case ExpressionParser::Node::VARIABLE: {
			// Note, this code should only run if the whole expression is only a variable.
			// Variable node inputs don't create a node each time, they are turned into connections in a later pass.
			// Here we need a pass-through node, so let's use `var + 0`. It's not a common case anyways.
			ProgramGraph::Node &pg_node = create_node(graph, db, VoxelGraphFunction::NODE_ADD);
			const ExpressionParser::VariableNode &vn =
					reinterpret_cast<const ExpressionParser::VariableNode &>(ep_node);
			to_connect.push_back({ vn.name, { pg_node.id, 0 } });
			ZN_ASSERT(pg_node.default_inputs.size() == 2);
			pg_node.default_inputs[1] = 0;
			expanded_node_ids.push_back(pg_node.id);
			return pg_node.id;
		}

		case ExpressionParser::Node::OPERATOR: {
			const ExpressionParser::OperatorNode &on =
					reinterpret_cast<const ExpressionParser::OperatorNode &>(ep_node);

			ZN_ASSERT(on.n0 != nullptr);
			ZN_ASSERT(on.n1 != nullptr);

			VoxelGraphFunction::NodeTypeID node_type_id;
			switch (on.op) {
				case ExpressionParser::OperatorNode::ADD:
					node_type_id = VoxelGraphFunction::NODE_ADD;
					break;
				case ExpressionParser::OperatorNode::SUBTRACT:
					node_type_id = VoxelGraphFunction::NODE_SUBTRACT;
					break;
				case ExpressionParser::OperatorNode::MULTIPLY:
					node_type_id = VoxelGraphFunction::NODE_MULTIPLY;
					break;
				case ExpressionParser::OperatorNode::DIVIDE:
					node_type_id = VoxelGraphFunction::NODE_DIVIDE;
					break;
				case ExpressionParser::OperatorNode::POWER:
					if (on.n1->type == ExpressionParser::Node::NUMBER) {
						// Attempt to use an optimized node if the power is constant
						const ExpressionParser::NumberNode &arg1 =
								static_cast<const ExpressionParser::NumberNode &>(*on.n1);

						const int pi = int(arg1.value);
						if (Math::is_equal_approx(arg1.value, pi) && pi >= 0) {
							// Constant positive integer
							ProgramGraph::Node &pg_node = create_node(graph, db, VoxelGraphFunction::NODE_POWI);
							expanded_node_ids.push_back(pg_node.id);

							ZN_ASSERT(pg_node.params.size() == 1);
							pg_node.params[0] = pi;

							ZN_ASSERT_RETURN_V(expand_input(graph, *on.n0, pg_node, 0, db, to_connect,
													   expanded_node_ids, functions),
									ProgramGraph::NULL_ID);

							return pg_node.id;
						}
					}
					// Fallback on generic power function
					node_type_id = VoxelGraphFunction::NODE_POW;
					break;
				default:
					ZN_CRASH();
					break;
			}

			ProgramGraph::Node &pg_node = create_node(graph, db, node_type_id);
			expanded_node_ids.push_back(pg_node.id);

			ZN_ASSERT_RETURN_V(expand_input(graph, *on.n0, pg_node, 0, db, to_connect, expanded_node_ids, functions),
					ProgramGraph::NULL_ID);

			ZN_ASSERT_RETURN_V(expand_input(graph, *on.n1, pg_node, 1, db, to_connect, expanded_node_ids, functions),
					ProgramGraph::NULL_ID);

			return pg_node.id;
		}

		case ExpressionParser::Node::FUNCTION: {
			const ExpressionParser::FunctionNode &fn =
					reinterpret_cast<const ExpressionParser::FunctionNode &>(ep_node);
			const ExpressionParser::Function *f = ExpressionParser::find_function_by_id(fn.function_id, functions);
			ZN_ASSERT(f != nullptr);
			const unsigned int arg_count = f->argument_count;

			ProgramGraph::Node &pg_node = create_node(graph, db, VoxelGraphFunction::NodeTypeID(fn.function_id));
			// TODO Optimization: per-function shortcuts

			for (unsigned int arg_index = 0; arg_index < arg_count; ++arg_index) {
				const ExpressionParser::Node *arg = fn.args[arg_index].get();
				ZN_ASSERT(arg != nullptr);
				ZN_ASSERT_RETURN_V(
						expand_input(graph, *arg, pg_node, arg_index, db, to_connect, expanded_node_ids, functions),
						ProgramGraph::NULL_ID);
			}

			return pg_node.id;
		}

		default:
			return ProgramGraph::NULL_ID;
	}
}

static VoxelGraphRuntime::CompilationResult expand_expression_node(ProgramGraph &graph, uint32_t original_node_id,
		ProgramGraph::PortLocation &expanded_output_port, std::vector<uint32_t> &expanded_nodes,
		const VoxelGraphNodeDB &type_db) {
	ZN_PROFILE_SCOPE();
	const ProgramGraph::Node &original_node = graph.get_node(original_node_id);
	ZN_ASSERT(original_node.params.size() != 0);
	const String code = original_node.params[0];
	const CharString code_utf8 = code.utf8();

	Span<const ExpressionParser::Function> functions = type_db.get_expression_parser_functions();

	// Extract the AST, so we can convert it into graph nodes,
	// and benefit from all features of range analysis and buffer processing
	ExpressionParser::Result parse_result = ExpressionParser::parse(code_utf8.get_data(), functions);

	if (parse_result.error.id != ExpressionParser::ERROR_NONE) {
		// Error in expression
		const std::string error_message_utf8 = ExpressionParser::to_string(parse_result.error);
		VoxelGraphRuntime::CompilationResult result;
		result.success = false;
		result.node_id = original_node_id;
		result.message = String(error_message_utf8.c_str());
		return result;
	}

	if (parse_result.root == nullptr) {
		// Expression is empty
		VoxelGraphRuntime::CompilationResult result;
		result.success = false;
		result.node_id = original_node_id;
		result.message = "Expression is empty";
		return result;
	}

	std::vector<ToConnect> to_connect;

	// Create nodes from the expression's AST and connect them together
	const uint32_t expanded_root_node_id =
			expand_node(graph, *parse_result.root, type_db, to_connect, expanded_nodes, functions);
	if (expanded_root_node_id == ProgramGraph::NULL_ID) {
		VoxelGraphRuntime::CompilationResult result;
		result.success = false;
		result.node_id = original_node_id;
		result.message = "Internal error";
		return result;
	}

	expanded_output_port = { expanded_root_node_id, 0 };

	// Add connections from outside the expression to entry nodes of the expression
	for (const ToConnect tc : to_connect) {
		unsigned int original_port_index;
		if (!original_node.find_input_port_by_name(tc.var_name, original_port_index)) {
			VoxelGraphRuntime::CompilationResult result;
			result.success = false;
			result.node_id = original_node_id;
			result.message = "Could not resolve expression variable from input ports";
			return result;
		}
		ZN_ASSERT(original_port_index < original_node.inputs.size());
		const ProgramGraph::Port &original_port = original_node.inputs[original_port_index];
		for (const ProgramGraph::PortLocation src : original_port.connections) {
			graph.connect(src, tc.dst);
		}
	}

	// Copy first because we'll remove the original node
	ZN_ASSERT(original_node.outputs.size() != 0);
	const ProgramGraph::Port original_output_port_copy = original_node.outputs[0];

	// Remove the original expression node
	graph.remove_node(original_node_id);

	// Add connections from the expression's final node.
	// Must be done at the end because adding two connections to the same input (old and new) is not allowed.
	for (const ProgramGraph::PortLocation dst : original_output_port_copy.connections) {
		graph.connect(expanded_output_port, dst);
	}

	VoxelGraphRuntime::CompilationResult result;
	result.success = true;
	return result;
}

VoxelGraphRuntime::CompilationResult expand_expression_nodes(
		ProgramGraph &graph, const VoxelGraphNodeDB &type_db, GraphRemappingInfo *remap_info) {
	ZN_PROFILE_SCOPE();
	const unsigned int initial_node_count = graph.get_nodes_count();

	// Gather expression node IDs first, as expansion could invalidate the iterator
	std::vector<uint32_t> expression_node_ids;
	graph.for_each_node([&expression_node_ids](ProgramGraph::Node &node) {
		if (node.type_id == VoxelGraphFunction::NODE_EXPRESSION) {
			expression_node_ids.push_back(node.id);
		}
	});

	std::vector<uint32_t> expanded_node_ids;

	for (const uint32_t node_id : expression_node_ids) {
		ProgramGraph::PortLocation expanded_output_port;
		expanded_node_ids.clear();
		const VoxelGraphRuntime::CompilationResult result =
				expand_expression_node(graph, node_id, expanded_output_port, expanded_node_ids, type_db);
		if (!result.success) {
			return result;
		}
		if (remap_info != nullptr) {
			add_remap(*remap_info, node_id, to_span(expanded_node_ids), expanded_output_port);
		}
	}

	// Expanding expression nodes may produce more nodes, not remove any
	ZN_ASSERT_RETURN_V(graph.get_nodes_count() >= initial_node_count,
			VoxelGraphRuntime::CompilationResult::make_error("Internal error"));

	VoxelGraphRuntime::CompilationResult result;
	result.success = true;
	return result;
}

struct NodePair {
	uint32_t node1_id; // On node1's branch
	uint32_t node2_id; // On node2's branch

	inline bool operator!=(const NodePair &other) const {
		return node1_id != other.node1_id || node2_id != other.node2_id;
	}
};

static bool is_node_equivalent(const ProgramGraph &graph, const ProgramGraph::Node &node1,
		const ProgramGraph::Node &node2, std::vector<NodePair> &equivalences) {
	if (node1.id == node2.id) {
		// They are the same node (this can happen while processing ancestors).
		return true;
	}
	if (node1.type_id != node2.type_id) {
		// Different type
		return false;
	}
	// Note, some nodes can have dynamic inputs, so we don't check node type specs, we check the nodes
	if (node1.inputs.size() != node2.inputs.size()) {
		// Different input count
		return false;
	}
	ZN_ASSERT_RETURN_V(node1.params.size() == node2.params.size(), false);
	for (unsigned int param_index = 0; param_index < node1.params.size(); ++param_index) {
		const Variant v1 = node1.params[param_index];
		const Variant v2 = node2.params[param_index];
		// Not checking objects for now. i.e two equivalent Noise instances will not be considered equivalent.
		if (v1 != v2) {
			// Different parameter
			return false;
		}
	}
	for (unsigned int input_index = 0; input_index < node1.inputs.size(); ++input_index) {
		const ProgramGraph::Port &node1_input = node1.inputs[input_index];
		const ProgramGraph::Port &node2_input = node2.inputs[input_index];
		if (node1_input.connections.size() != node2_input.connections.size()) {
			return false;
		}
		ZN_ASSERT_RETURN_V_MSG(
				node1_input.connections.size() <= 1, false, "Multiple input connections isn't supported");
		// TODO Some nodes like `*` and `+` have unordered inputs, we need to handle that
		if (node1_input.connections.size() == 0) {
			// Continuing the paranoia here, but that's because Godot doesn't define `_DEBUG` (and I can't define it in
			// my module without failing to link), so standard library bound checks are in the toilet
			ZN_ASSERT(node1.default_inputs.size() == node1.inputs.size());
			ZN_ASSERT(node2.default_inputs.size() == node2.inputs.size());
			// No ancestor, check default inputs (autoconnect is ignored, it must have been applied earlier)
			const Variant v1 = node1.default_inputs[input_index];
			const Variant v2 = node2.default_inputs[input_index];
			if (v1 != v2) {
				// Different default inputs
				return false;
			}
		} else {
			const ProgramGraph::PortLocation &node1_src = node1_input.connections[0];
			const ProgramGraph::PortLocation &node2_src = node2_input.connections[0];
			if (node1_src.port_index != node2_src.port_index) {
				// Different ancestor output
				return false;
			}
			const ProgramGraph::Node &ancestor1 = graph.get_node(node1_src.node_id);
			const ProgramGraph::Node &ancestor2 = graph.get_node(node2_src.node_id);
			if (!is_node_equivalent(graph, ancestor1, ancestor2, equivalences)) {
				// Different ancestors
				equivalences.clear();
				return false;
			}
		}
	}
	NodePair equivalence{ node1.id, node2.id };
#ifdef DEBUG_ENABLED
	for (const NodePair &p : equivalences) {
		ZN_ASSERT(p != equivalence);
	}
#endif
	equivalences.push_back(equivalence);
	return true;
}

static void merge_node(ProgramGraph &graph, uint32_t node1_id, uint32_t node2_id, GraphRemappingInfo *remap_info) {
	const ProgramGraph::Node &node1 = graph.get_node(node1_id);
	const ProgramGraph::Node &node2 = graph.get_node(node2_id);
	ZN_ASSERT_RETURN(node1.type_id == node2.type_id);
	ZN_ASSERT_RETURN(node1.outputs.size() == node2.outputs.size());
	// Remove 2, keep 1
	for (unsigned int output_index = 0; output_index < node2.outputs.size(); ++output_index) {
		// Remove output connections, re-create them on the equivalent node.
		// Copy output connections because they will get modified while iterating.
		std::vector<ProgramGraph::PortLocation> dsts = node2.outputs[output_index].connections;
		for (ProgramGraph::PortLocation dst : dsts) {
			graph.disconnect(ProgramGraph::PortLocation{ node2_id, output_index }, dst);
			graph.connect(ProgramGraph::PortLocation{ node1_id, output_index }, dst);
		}
	}
	if (remap_info != nullptr) {
		add_remap(*remap_info, node2_id, node1_id, node1.outputs.size());
	}
	graph.remove_node(node2_id);
}

// Finds nodes with equivalent parameters and equivalent ancestors, so they can be merged into a single branch.
// This should be done preferably after expanding expressions and macros.
// Automating this allows to create macros that use similar operations without loosing performance due to repetition.
// For example, a SphereHeightNoise macro will want to normalize (X,Y,Z). Other branches may want to do this too,
// so we should share that operation, but it's harder to do so with self-contained branches. So it's easier if that
// can be delegated to an automated process.
static void merge_equivalences(ProgramGraph &graph, GraphRemappingInfo *remap_info) {
	ZN_PROFILE_SCOPE();
	std::vector<uint32_t> node_ids;
	graph.get_node_ids(node_ids);

	// Declaring here so we dont reallocate memory too much
	std::vector<NodePair> equivalences;

	// For each unique pair of nodes
	for (unsigned int i = 0; i < node_ids.size(); ++i) {
		// Nodes in the list might get removed in the process so we test if they still exist
		const uint32_t node1_id = node_ids[i];
		const ProgramGraph::Node *node1 = graph.try_get_node(node1_id);
		if (node1 == nullptr) {
			continue;
		}
		for (unsigned int j = i + 1; j < node_ids.size(); ++j) {
			const uint32_t node2_id = node_ids[j];
			const ProgramGraph::Node *node2 = graph.try_get_node(node2_id);
			if (node2 == nullptr) {
				// Can have been merged
				continue;
			}
			equivalences.clear();
			// Is the pair equivalent?
			if (is_node_equivalent(graph, *node1, *node2, equivalences)) {
				// Merge nodes to share their outputs
				for (NodePair equivalence : equivalences) {
					merge_node(graph, equivalence.node1_id, equivalence.node2_id, remap_info);
				}
				if (graph.try_get_node(node1_id) == nullptr) {
					// Node 1 has been merged, stop looking for equivalences with it
					break;
				}
			}
		}
	}
}

// For each node with auto-connect enabled, if they have non-connected ports supporting auto-connect, connects them to a
// default input node. If no such node exists in the graph, it is created.
static void apply_auto_connects(ProgramGraph &graph, const VoxelGraphNodeDB &type_db) {
	// Copy ids first because we might create new nodes
	std::vector<uint32_t> node_ids;
	graph.get_node_ids(node_ids);

	for (const uint32_t node_id : node_ids) {
		const ProgramGraph::Node &node = graph.get_node(node_id);

		if (node.autoconnect_default_inputs == false) {
			// Explicit constants will be used instead
			continue;
		}

		for (unsigned int input_index = 0; input_index < node.inputs.size(); ++input_index) {
			const ProgramGraph::Port &input_port = node.inputs[input_index];
			if (input_port.connections.size() > 0) {
				// Already connected
				continue;
			}
			//const VoxelGraphNodeDB::NodeType &type = type_db.get_type(node.type_id);
			//ZN_ASSERT(node.inputs.size() == type.inputs.size());
			//const VoxelGraphNodeDB::AutoConnect auto_connect = type.inputs[input_index].auto_connect;
			const VoxelGraphFunction::AutoConnect auto_connect =
					VoxelGraphFunction::AutoConnect(input_port.autoconnect_hint);
			VoxelGraphFunction::NodeTypeID src_type;
			switch (auto_connect) {
				case VoxelGraphFunction::AUTO_CONNECT_X:
					src_type = VoxelGraphFunction::NODE_INPUT_X;
					break;
				case VoxelGraphFunction::AUTO_CONNECT_Y:
					src_type = VoxelGraphFunction::NODE_INPUT_Y;
					break;
				case VoxelGraphFunction::AUTO_CONNECT_Z:
					src_type = VoxelGraphFunction::NODE_INPUT_Z;
					break;
				case VoxelGraphFunction::AUTO_CONNECT_NONE:
					continue;
				default:
					ZN_PRINT_ERROR("Unhandled autoconnect");
					continue;
			}
			// Graph input node instances are all equivalent, so we can pick any
			uint32_t src_node_id = graph.find_node_by_type(src_type);
			if (src_node_id == ProgramGraph::NULL_ID) {
				// Not found, create it then
				const ProgramGraph::Node *src_node =
						create_node_internal(graph, src_type, Vector2(), graph.generate_node_id(), false);
				ZN_ASSERT_CONTINUE(src_node != nullptr);
				src_node_id = src_node->id;
			}
			graph.connect(
					ProgramGraph::PortLocation{ src_node_id, 0 }, ProgramGraph::PortLocation{ node_id, input_index });
		}
	}
}

void try_simplify_clamp_node(ProgramGraph &graph, const ProgramGraph::Node &node, const VoxelGraphNodeDB &type_db,
		GraphRemappingInfo *remap_info) {
	ZN_ASSERT(node.inputs.size() == 3);

	const uint32_t clamp_x_input_id = 0;
	const uint32_t clamp_output_id = 0;
	const uint32_t clamp_min_input_id = 1;
	const uint32_t clamp_max_input_id = 2;

	const uint32_t clampc_x_input_id = 0;
	const uint32_t clampc_output_id = 0;
	const uint32_t clampc_min_param_id = 0;
	const uint32_t clampc_max_param_id = 1;

	if (node.inputs[clamp_min_input_id].connections.size() == 0 &&
			node.inputs[clamp_max_input_id].connections.size() == 0) {
		// Can be replaced with a clamp version with constant bounds

		ZN_ASSERT(node.default_inputs.size() == node.inputs.size());

		const float minv = node.default_inputs[clamp_min_input_id];
		const float maxv = node.default_inputs[clamp_max_input_id];

		// Create new node
		ProgramGraph::Node &clampc_node = create_node(graph, type_db, VoxelGraphFunction::NODE_CLAMP_C);

		// Assign new node params
		clampc_node.params[clampc_min_param_id] = minv;
		clampc_node.params[clampc_max_param_id] = maxv;

		// Connect inputs of new node
		const ProgramGraph::Port &clamp_input = node.inputs[clamp_x_input_id];
		for (const ProgramGraph::PortLocation &src : clamp_input.connections) {
			graph.connect(src, ProgramGraph::PortLocation{ clampc_node.id, clampc_x_input_id });
		}

		// Making a copy because we first need to disconnect those connections
		const std::vector<ProgramGraph::PortLocation> clamp_output_connections =
				node.outputs[clamp_output_id].connections;
		for (const ProgramGraph::PortLocation &dst : clamp_output_connections) {
			graph.disconnect(ProgramGraph::PortLocation{ node.id, clamp_output_id }, dst);
		}

		// Connect outputs of new node
		for (const ProgramGraph::PortLocation &dst : clamp_output_connections) {
			graph.connect(ProgramGraph::PortLocation{ clampc_node.id, clampc_output_id }, dst);
		}

		// Update remaps for debug tracing
		if (remap_info != nullptr) {
			add_remap(*remap_info, node.id, clampc_node.id, node.outputs.size());
		}

		// Remove old node
		graph.remove_node(node.id);
		// From this point, `node` is invalid.
	}
}

void replace_simplifiable_nodes(ProgramGraph &graph, const VoxelGraphNodeDB &type_db, GraphRemappingInfo *remap_info) {
	std::vector<uint32_t> node_ids;
	// TODO Optimize: only gather node IDs we are interested in?
	graph.get_node_ids(node_ids);

	for (const uint32_t &node_id : node_ids) {
		const ProgramGraph::Node &node = graph.get_node(node_id);

		if (node.type_id == VoxelGraphFunction::NODE_CLAMP) {
			try_simplify_clamp_node(graph, node, type_db, remap_info);
		}
	}
}

ProgramGraph::Node *duplicate_node(ProgramGraph &dst_graph, const ProgramGraph::Node &src_node) {
	ProgramGraph::Node *dst_node = dst_graph.create_node(src_node.type_id);
	ZN_ASSERT(dst_node != nullptr);
	dst_node->name = src_node.name;
	dst_node->inputs.resize(src_node.inputs.size());
	dst_node->outputs.resize(src_node.outputs.size());
	dst_node->default_inputs = src_node.default_inputs;
	dst_node->params = src_node.params;
	// dst_node->gui_position = src_node.gui_position;
	// dst_node->gui_size = src_node.gui_size;
	dst_node->autoconnect_default_inputs = src_node.autoconnect_default_inputs;
	return dst_node;
}

// Replaces a function node with its contents in place, with equivalent connections to its surroundings.
void expand_function(
		ProgramGraph &graph, uint32_t node_id, const VoxelGraphNodeDB &type_db, GraphRemappingInfo *remap_info) {
	ZN_PROFILE_SCOPE();
	const ProgramGraph::Node &fnode = graph.get_node(node_id);
	ZN_ASSERT(fnode.type_id == VoxelGraphFunction::NODE_FUNCTION);
	ZN_ASSERT(fnode.params.size() >= 1);
	Ref<VoxelGraphFunction> function = fnode.params[0];
	ZN_ASSERT(function.is_valid());
	const ProgramGraph &fgraph = function->get_graph();

	std::unordered_map<uint32_t, uint32_t> fn_to_expanded_node_ids;
	std::vector<uint32_t> nested_func_node_ids;

	// Copy nodes into the graph
	fgraph.for_each_node_const(
			[&graph, &type_db, &fn_to_expanded_node_ids, &nested_func_node_ids](const ProgramGraph::Node &src_node) {
				const VoxelGraphNodeDB::NodeType &node_type = type_db.get_type(src_node.type_id);
				// Ignore input and output nodes, we only copy what's in between
				if (node_type.category == VoxelGraphNodeDB::CATEGORY_INPUT) {
					return;
				}
				if (node_type.category == VoxelGraphNodeDB::CATEGORY_OUTPUT) {
					return;
				}
				const ProgramGraph::Node *expanded_node = duplicate_node(graph, src_node);
				fn_to_expanded_node_ids[src_node.id] = expanded_node->id;

				if (expanded_node->type_id == VoxelGraphFunction::NODE_FUNCTION) {
					nested_func_node_ids.push_back(expanded_node->id);
				}
			});

	// Copy internal connections
	for (auto it = fn_to_expanded_node_ids.begin(); it != fn_to_expanded_node_ids.end(); ++it) {
		const uint32_t fn_node_id = it->first;
		const uint32_t ex_node_id = it->second;

		const ProgramGraph::Node &fn_node = fgraph.get_node(fn_node_id);

		for (unsigned int i = 0; i < fn_node.outputs.size(); ++i) {
			const ProgramGraph::Port &fn_port = fn_node.outputs[i];

			for (const ProgramGraph::PortLocation fn_dst : fn_port.connections) {
				auto it2 = fn_to_expanded_node_ids.find(fn_dst.node_id);
				if (it2 == fn_to_expanded_node_ids.end()) {
					continue;
				}
				graph.connect(ProgramGraph::PortLocation{ ex_node_id, i },
						ProgramGraph::PortLocation{ it2->second, fn_dst.port_index });
			}
		}
	}

	Span<const VoxelGraphFunction::Port> func_inputs = function->get_input_definitions();
	Span<const VoxelGraphFunction::Port> func_outputs = function->get_output_definitions();

	std::vector<uint32_t> input_node_ids;
	std::vector<uint32_t> output_node_ids;

	ZN_ASSERT(func_inputs.size() == fnode.inputs.size());
	ZN_ASSERT(func_outputs.size() == fnode.outputs.size());

	// Disconnect outputs of the function node, because we are going to replace them.
	// (destination ports are not allowed to have two connections at a given time)
	std::vector<ProgramGraph::Port> fnode_outputs = fnode.outputs;
	for (unsigned int output_index = 0; output_index < fnode_outputs.size(); ++output_index) {
		const ProgramGraph::Port &port = fnode_outputs[output_index];
		ProgramGraph::PortLocation src{ fnode.id, output_index };
		for (const ProgramGraph::PortLocation &dst : port.connections) {
			graph.disconnect(src, dst);
		}
	}

	// TODO What about autoconnect? Currently functions must use explicit input XYZ nodes, otherwise it will connect
	// these inputs to the global XYZ inputs (after everything is unpacked), not the inputs of the function

	// Add connections coming to the function's unpacked nodes.
	// For each input of the function
	for (unsigned int input_index = 0; input_index < fnode.inputs.size(); ++input_index) {
		const ProgramGraph::Port &port = fnode.inputs[input_index];
		if (port.connections.size() == 0) {
			continue;
		}
		ZN_ASSERT_MSG(port.connections.size() == 1, "Input nodes are expected to have only 1 input");
		const ProgramGraph::PortLocation src = port.connections[0];
		const VoxelGraphFunction::Port &fport = func_inputs[input_index];

		// TODO Optimize: this will iterate every node of the graph several times, maybe we can do better without adding
		// too much code?
		input_node_ids.clear();
		function->get_input_node_ids(input_node_ids, input_index);

		// For each input nodes in the function representing this input
		for (const uint32_t fid : input_node_ids) {
			const ProgramGraph::Node &func_internal_input_node = fgraph.get_node(fid);
			ZN_ASSERT_MSG(
					func_internal_input_node.outputs.size() == 1, "Input nodes are expected to have only 1 output");
			const ProgramGraph::Port &fio = func_internal_input_node.outputs[0];

			// For each connection coming from that input node
			for (const ProgramGraph::PortLocation fdst : fio.connections) {
				auto it = fn_to_expanded_node_ids.find(fdst.node_id);

				if (it != fn_to_expanded_node_ids.end()) {
					// The input is connected to an internal node of the function.
					// Create equivalent connection to unpacked nodes
					graph.connect(src, ProgramGraph::PortLocation{ it->second, fdst.port_index });

				} else {
					// The input is connected directly to an output of the function?
					const ProgramGraph::Node &func_internal_output_node = fgraph.get_node(fdst.node_id);
					const VoxelGraphNodeDB::NodeType func_internal_output_node_type =
							type_db.get_type(func_internal_output_node.type_id);
					ZN_ASSERT(func_internal_output_node_type.category == VoxelGraphNodeDB::CATEGORY_OUTPUT);

					// Find the output index corresponding to the output node
					// TODO Optimize: perhaps we should gather all node IDs per port before I/O passes
					struct L {
						static bool find_output_index_from_node(unsigned int output_count,
								std::vector<uint32_t> &output_node_ids, VoxelGraphFunction &func,
								uint32_t output_node_id, unsigned int &out_output_index) {
							unsigned int output_index = 0;
							bool found = false;
							for (; output_index < output_count && !found; ++output_index) {
								output_node_ids.clear();
								func.get_output_node_ids(output_node_ids, output_index);
								for (const uint32_t id : output_node_ids) {
									if (id == output_node_id) {
										out_output_index = output_index;
										return true;
									}
								}
							}
							return false;
						}
					};

					unsigned int output_index;
					const bool found = L::find_output_index_from_node(
							fnode_outputs.size(), output_node_ids, **function, fdst.node_id, output_index);
					// We expect to find the output index. If we don't, that output node is invalid maybe, or outputs
					// aren't correctly configured.
					ZN_ASSERT(found);

					// We expect the output index to correspond to the node's data. If not, maybe there was a desync or
					// bug?
					ZN_ASSERT(output_index < fnode_outputs.size());

					//                   B                     B
					//                  /                     /
					//  A --- In --- Out --- C    Becomes:   A --- C
					//            ^     \                     \ 
					//                   D                     D

					// Create connections
					const ProgramGraph::Port &oport = fnode_outputs[output_index];
					for (const ProgramGraph::PortLocation &dst : oport.connections) {
						graph.connect(src, dst);
					}
				}
			}
		}
	}

	std::vector<ProgramGraph::PortLocation> output_locations;

	// Add connections coming from the function's unpacked nodes.
	for (unsigned int output_index = 0; output_index < fnode_outputs.size(); ++output_index) {
		const ProgramGraph::Port &port = fnode_outputs[output_index];
		if (port.connections.size() == 0) {
			// That output isn't connected outside the function
			continue;
		}
		//const VoxelGraphFunction::Port fport = func_outputs[output_index];
		output_node_ids.clear();
		function->get_output_node_ids(output_node_ids, output_index);
		// An output node can only appear once
		ZN_ASSERT(output_node_ids.size() == 1);
		const ProgramGraph::Node &inner_fnode = fgraph.get_node(output_node_ids[0]);
		ZN_ASSERT(inner_fnode.inputs.size() == 1);
		const ProgramGraph::Port &foi = inner_fnode.inputs[0];
		if (foi.connections.size() == 0) {
			// That output isn't connected inside the function
			continue;
		}
		ZN_ASSERT(foi.connections.size() == 1);
		const ProgramGraph::PortLocation fsrc = foi.connections[0];
		auto it = fn_to_expanded_node_ids.find(fsrc.node_id);
		if (it == fn_to_expanded_node_ids.end()) {
			// This output isn't connected to an internal node of the function. Maybe it's connected directly to an
			// input? If so, we may skip it, because we handle this case in the inputs pass.
			const ProgramGraph::Node &inode = fgraph.get_node(fsrc.node_id);
			const VoxelGraphNodeDB::NodeType inode_type = type_db.get_type(inode.type_id);
			ZN_ASSERT(inode_type.category == VoxelGraphNodeDB::CATEGORY_INPUT);
			continue;
		}
		for (const ProgramGraph::PortLocation dst : port.connections) {
			graph.connect(ProgramGraph::PortLocation{ it->second, fsrc.port_index }, dst);
		}

		if (remap_info != nullptr) {
			output_locations.push_back(ProgramGraph::PortLocation{ it->second, fsrc.port_index });
		}
	}

	if (remap_info != nullptr) {
		std::vector<uint32_t> expanded_node_ids;
		for (auto it = fn_to_expanded_node_ids.begin(); it != fn_to_expanded_node_ids.end(); ++it) {
			const uint32_t ex_node_id = it->second;
			const ProgramGraph::Node &node = graph.get_node(ex_node_id);
			// Don't add remaps to a function node, because they will be expanded anyways
			if (node.type_id == VoxelGraphFunction::NODE_FUNCTION) {
				continue;
			}
			expanded_node_ids.push_back(ex_node_id);
		}

		add_remap(*remap_info, node_id, to_span(expanded_node_ids), to_span(output_locations));
	}

	// Remove function node
	graph.remove_node(fnode.id);
	// From this point, `fnode` is invalid.

	// Expand nested functions
	for (const uint32_t nested_node_id : nested_func_node_ids) {
		expand_function(graph, nested_node_id, type_db, remap_info);
	}
}

void expand_functions(ProgramGraph &graph, const VoxelGraphNodeDB &type_db, GraphRemappingInfo *remap_info) {
	std::vector<uint32_t> func_node_ids;

	graph.for_each_node_const([&func_node_ids](const ProgramGraph::Node &node) {
		if (node.type_id == VoxelGraphFunction::NODE_FUNCTION) {
			func_node_ids.push_back(node.id);
		}
	});

	for (const uint32_t node_id : func_node_ids) {
		expand_function(graph, node_id, type_db, remap_info);
	}
}

VoxelGraphRuntime::CompilationResult expand_graph(const ProgramGraph &graph, ProgramGraph &expanded_graph,
		const VoxelGraphNodeDB &type_db, GraphRemappingInfo *remap_info) {
	// First make a copy of the graph which we'll modify
	expanded_graph.copy_from(graph, false);

	expand_functions(expanded_graph, type_db, remap_info);

	apply_auto_connects(expanded_graph, type_db);

	const VoxelGraphRuntime::CompilationResult expand_result =
			expand_expression_nodes(expanded_graph, type_db, remap_info);
	if (!expand_result.success) {
		return expand_result;
	}

	merge_equivalences(expanded_graph, remap_info);
	replace_simplifiable_nodes(expanded_graph, type_db, remap_info);

	return expand_result;
}

VoxelGraphRuntime::CompilationResult VoxelGraphRuntime::compile(const ProgramGraph &p_graph, bool debug) {
	ZN_PROFILE_SCOPE();

	const VoxelGraphNodeDB &type_db = VoxelGraphNodeDB::get_singleton();

	GraphRemappingInfo remap_info;
	ProgramGraph expanded_graph;
	const VoxelGraphRuntime::CompilationResult expand_result =
			expand_graph(p_graph, expanded_graph, type_db, &remap_info);
	if (!expand_result.success) {
		return expand_result;
	}

	VoxelGraphRuntime::CompilationResult result = _compile(expanded_graph, debug, type_db);
	if (!result.success) {
		clear();
	}

	for (PortRemap r : remap_info.user_to_expanded_ports) {
		_program.user_port_to_expanded_port.insert({ r.original, r.expanded });
	}
	for (ExpandedNodeRemap r : remap_info.expanded_to_user_node_ids) {
		_program.expanded_node_id_to_user_node_id.insert({ r.expanded_node_id, r.original_node_id });
	}
	// Remap debug nodes from the execution map to user-facing ones
	for (uint32_t &debug_node_id : _program.default_execution_map.debug_nodes) {
		auto it = _program.expanded_node_id_to_user_node_id.find(debug_node_id);
		if (it != _program.expanded_node_id_to_user_node_id.end()) {
			debug_node_id = it->second;
		}
	}

	result.expanded_nodes_count = expanded_graph.get_nodes_count();
	return result;
}

// Optimize parts of the graph that only depend on X and Z,
// so they can be moved in the outer loop when blocks are generated, running less times.
// Moves them all at the beginning.
static uint32_t move_xz_operations_up(std::vector<uint32_t> &order, const ProgramGraph &graph) {
	std::vector<uint32_t> immediate_deps;
	std::unordered_set<uint32_t> nodes_depending_on_y;
	std::vector<uint32_t> order_xz;
	std::vector<uint32_t> order_xzy;

	for (const uint32_t node_id : order) {
		const ProgramGraph::Node &node = graph.get_node(node_id);

		bool depends_on_y = false;

		if (node.type_id == VoxelGraphFunction::NODE_INPUT_Y) {
			nodes_depending_on_y.insert(node_id);
			depends_on_y = true;
		}

		if (!depends_on_y) {
			immediate_deps.clear();
			graph.find_immediate_dependencies(node_id, immediate_deps);

			for (const uint32_t dep_node_id : immediate_deps) {
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

	const uint32_t xzy_start_index = order_xz.size();

	//#ifdef DEBUG_ENABLED
	//		const uint32_t order_xz_raw_size = order_xz.size();
	//		const uint32_t *order_xz_raw = order_xz.data();
	//		const uint32_t order_xzy_raw_size = order_xzy.size();
	//		const uint32_t *order_xzy_raw = order_xzy.data();
	//#endif

	size_t i = 0;
	for (const uint32_t node_id : order_xz) {
		order[i++] = node_id;
	}
	for (const uint32_t node_id : order_xzy) {
		order[i++] = node_id;
	}
	ZN_ASSERT(i == order.size());

	return xzy_start_index;
}

static void compute_node_execution_order(
		std::vector<uint32_t> &order, const ProgramGraph &graph, bool debug, const VoxelGraphNodeDB &type_db) {
	std::vector<uint32_t> terminal_nodes;

	// Not using the generic `get_terminal_nodes` function because our terminal nodes do have outputs
	graph.for_each_node_const([&terminal_nodes, &type_db](const ProgramGraph::Node &node) {
		const VoxelGraphNodeDB::NodeType &type = type_db.get_type(node.type_id);
		if (type.category == VoxelGraphNodeDB::CATEGORY_OUTPUT) {
			terminal_nodes.push_back(node.id);
		}
	});

	if (!debug) {
		// Exclude debug nodes
		unordered_remove_if(terminal_nodes, [&graph, &type_db](uint32_t node_id) {
			const ProgramGraph::Node &node = graph.get_node(node_id);
			const VoxelGraphNodeDB::NodeType &type = type_db.get_type(node.type_id);
			return type.debug_only;
		});
	}

	graph.find_dependencies(terminal_nodes, order);
}

VoxelGraphRuntime::CompilationResult VoxelGraphRuntime::_compile(
		const ProgramGraph &graph, bool debug, const VoxelGraphNodeDB &type_db) {
	ZN_PROFILE_SCOPE();
	clear();

	std::vector<uint32_t> order;
	compute_node_execution_order(order, graph, debug, type_db);

	const uint32_t xzy_start_index = move_xz_operations_up(order, graph);

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
			bs.is_pinned = false;
			bs.users_count = 0;
			buffer_specs.push_back(bs);
			return a;
		}

		uint16_t add_constant(float v, bool buffer_required) {
			const unsigned int a = next_address;
			++next_address;
			BufferSpec bs;
			bs.address = a;
			bs.constant_value = v;
			bs.is_binding = false;
			bs.is_constant = true;
			bs.is_pinned = buffer_required;
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
	std::unordered_map<uint32_t, uint32_t> node_id_to_dependency_graph;
	std::vector<uint16_t> input_buffer_indices;

	// Run through each node in order, and turn them into program instructions
	for (size_t order_index = 0; order_index < order.size(); ++order_index) {
		const uint32_t node_id = order[order_index];
		const ProgramGraph::Node &node = graph.get_node(node_id);
		const VoxelGraphNodeDB::NodeType &type = type_db.get_type(node.type_id);

		ZN_ASSERT(node.inputs.size() == type.inputs.size());
		ZN_ASSERT(node.outputs.size() == type.outputs.size());

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
			// TODO Get rid of constant nodes, replace them with default inputs wherever they are used?
			case VoxelGraphFunction::NODE_CONSTANT: {
				ZN_ASSERT(type.outputs.size() == 1);
				ZN_ASSERT(type.params.size() == 1);
				const uint16_t a = mem.add_constant(node.params[0].operator float(), true);
				_program.output_port_addresses[ProgramGraph::PortLocation{ node_id, 0 }] = a;
				// Technically not an input or an output, but is a dependency regardless so treat it like an input
				dg_node.is_input = true;
				continue;
			}

			// Input nodes can appear multiple times in the graph, for convenience.
			// Multiple instances of the same node will refer to the same data.
			case VoxelGraphFunction::NODE_INPUT_X:
				_program.output_port_addresses[ProgramGraph::PortLocation{ node_id, 0 }] = _program.x_input_address;
				dg_node.is_input = true;
				continue;

			case VoxelGraphFunction::NODE_INPUT_Y:
				_program.output_port_addresses[ProgramGraph::PortLocation{ node_id, 0 }] = _program.y_input_address;
				dg_node.is_input = true;
				continue;

			case VoxelGraphFunction::NODE_INPUT_Z:
				_program.output_port_addresses[ProgramGraph::PortLocation{ node_id, 0 }] = _program.z_input_address;
				dg_node.is_input = true;
				continue;

			case VoxelGraphFunction::NODE_INPUT_SDF:
				if (_program.sdf_input_address == -1) {
					_program.sdf_input_address = mem.add_binding();
				}
				_program.output_port_addresses[ProgramGraph::PortLocation{ node_id, 0 }] = _program.sdf_input_address;
				dg_node.is_input = true;
				continue;

			case VoxelGraphFunction::NODE_SDF_PREVIEW: {
				if (!debug) {
					ZN_PRINT_WARNING("Found preview node when compiling graph in non-debug mode. That node should not "
									 "have been present. Bug?");
				}
				auto it = _program.output_port_addresses.find(ProgramGraph::PortLocation{ node_id, 0 });
				if (it != _program.output_port_addresses.end()) {
					const uint16_t a = it->second;
					ZN_ASSERT(a < _program.buffer_specs.size());
					BufferSpec &src_buffer_spec = _program.buffer_specs[a];
					// Add a fake user, we want to see their result.
					// Pinning would work too, but it allocates more buffers.
					//src_buffer_spec.is_pinned = true;
					++src_buffer_spec.users_count;
				}
				continue;
			}
		}

		// Add actual operation

		ZN_ASSERT(node.type_id <= std::numeric_limits<uint16_t>::max());

		if (order_index == xzy_start_index) {
			_program.default_execution_map.xzy_start_index = _program.default_execution_map.operations.size();
		}
		_program.default_execution_map.operations.push_back(
				ExecutionMap::OperationInfo{ uint16_t(operations.size()), 0 });
		if (debug) {
			// Will be remapped later if the node is an expanded one
			_program.default_execution_map.debug_nodes.push_back(node_id);
		}

		operations.push_back(node.type_id);

		// Inputs and outputs use a convention so we can have generic code for them.
		// Parameters are more specific, and may be affected by alignment so better just do them by hand

		// Add inputs
		for (size_t j = 0; j < type.inputs.size(); ++j) {
			const VoxelGraphNodeDB::Port &port = type.inputs[j];
			uint16_t a;

			if (node.inputs[j].connections.size() == 0) {
				// No input, default it
				ZN_ASSERT(j < node.default_inputs.size());
				float defval = node.default_inputs[j];
				a = mem.add_constant(defval, port.require_input_buffer_when_constant);

			} else {
				ProgramGraph::PortLocation src_port = node.inputs[j].connections[0];
				auto address_it = _program.output_port_addresses.find(src_port);
				// Previous node ports must have been registered
				ZN_ASSERT(address_it != _program.output_port_addresses.end());
				a = address_it->second;

				// Register dependency
				auto it = node_id_to_dependency_graph.find(src_port.node_id);
				ZN_ASSERT(it != node_id_to_dependency_graph.end());
				ZN_ASSERT(it->second < _program.dependency_graph.nodes.size());
				_program.dependency_graph.dependencies.push_back(it->second);
				++dg_node.end_dependency;
			}

			operations.push_back(a);

			ZN_ASSERT(a < _program.buffer_specs.size());
			BufferSpec &bs = _program.buffer_specs[a];
			++bs.users_count;

			input_buffer_indices.push_back(a);
		}

		// Add outputs
		for (size_t j = 0; j < type.outputs.size(); ++j) {
			// Note, outputs of output nodes could be pinned, however since they are given a fake user, their lifespan
			// is undeterminate and will never be re-used by memory allocation. This is better than pinning, because the
			// buffer can be re-used several times before stopping at the output, while pinned buffers are always unique
			// to their locations.
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
		params_copy.reserve(node.params.size());
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

			params_copy.push_back(v);
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
			ZN_ASSERT(params_size <= std::numeric_limits<uint16_t>::max());
			ZN_ASSERT(params_size_index < operations.size());
			operations[params_size_index] = params_size;
		}

		if (type.category == VoxelGraphNodeDB::CATEGORY_OUTPUT) {
			ZN_ASSERT(node.outputs.size() == 1);

			if (_program.outputs_count == _program.outputs.size()) {
				CompilationResult result;
				result.success = false;
				result.message = ZN_TTR("Maximum number of outputs has been reached");
				result.node_id = node_id;
				return result;
			}

			{
				auto address_it = _program.output_port_addresses.find(ProgramGraph::PortLocation{ node_id, 0 });
				// Previous node ports must have been registered
				ZN_ASSERT(address_it != _program.output_port_addresses.end());
				OutputInfo &output_info = _program.outputs[_program.outputs_count];
				output_info.buffer_address = address_it->second;
				output_info.dependency_graph_node_index = dg_node_index;
				output_info.node_id = node_id;
				++_program.outputs_count;
			}

			// Add fake user for output ports so they can pass the local users check in optimizations
			for (unsigned int j = 0; j < type.outputs.size(); ++j) {
				const ProgramGraph::PortLocation loc{ node_id, j };
				auto address_it = _program.output_port_addresses.find(loc);
				ZN_ASSERT(address_it != _program.output_port_addresses.end());
				BufferSpec &bs = _program.buffer_specs[address_it->second];
				// Not expecting existing users on that port
				ZN_ASSERT_RETURN_V(bs.users_count == 0, CompilationResult());
				++bs.users_count;
			}
		}

#ifdef VOXEL_DEBUG_GRAPH_PROG_SENTINEL
		// Append a special value after each operation
		append(operations, VOXEL_DEBUG_GRAPH_PROG_SENTINEL);
#endif
	}

	_program.buffer_count = mem.next_address;

	// Pin buffers from the XZ group that are read by operations of the XZY group.
	// Buffer data coming from the XZ group must be pinned if it is read by the XZY group,
	// because it is re-used across multiple executions.
	{
		Span<BufferSpec> buffer_specs = to_span(_program.buffer_specs);

		// For each node of the XZY group
		for (unsigned int order_index = xzy_start_index; order_index < order.size(); ++order_index) {
			const uint32_t node_id = order[order_index];
			const ProgramGraph::Node &node = graph.get_node(node_id);
			//const VoxelGraphNodeDB::NodeType &type = type_db.get_type(node.type_id);

			for (const ProgramGraph::Port &input : node.inputs) {
				if (input.connections.size() == 0) {
					continue;
				}
				const ProgramGraph::PortLocation src_port = input.connections[0];

				// Find if the source node is part of the XZ group
				bool found = false;
				for (unsigned int i = 0; i < xzy_start_index; ++i) {
					if (order[i] == src_port.node_id) {
						found = true;
						break;
					}
				}
				if (!found) {
					continue;
				}

				auto address_it = _program.output_port_addresses.find(src_port);
				// Previous node ports must have been registered
				ZN_ASSERT(address_it != _program.output_port_addresses.end());
				BufferSpec &src_buffer_spec = buffer_specs[address_it->second];
				src_buffer_spec.is_pinned = true;
			}
		}
	}

	// Assign buffer datas
	{
		struct DataHelper {
			std::vector<uint16_t> free_indices;
			struct Data {
				uint16_t usages;
				bool pinned;
			};
			std::vector<Data> datas;

			uint16_t allocate(uint16_t users, bool pinned) {
				ZN_ASSERT(users > 0);
				// Note, pinned buffers must have unique data, so we may not re-use a previous buffer for them
				if (free_indices.size() == 0 || pinned) {
					const uint16_t i = datas.size();
					datas.push_back(Data{ users, pinned });
					return i;
				} else {
					const uint16_t i = free_indices[free_indices.size() - 1];
					free_indices.pop_back();
					ZN_ASSERT(i < datas.size());
					Data &d = datas[i];
					// Must not re-use a pinned buffer
					ZN_ASSERT(!d.pinned);
					d.usages = users;
					return i;
				}
			}

			void unref(uint16_t i) {
				ZN_ASSERT(i < datas.size());
				Data &d = datas[i];
				ZN_ASSERT(!d.pinned);
				ZN_ASSERT(d.usages > 0);
				--d.usages;
				if (d.usages == 0) {
					free_indices.push_back(i);
				}
			}
		};

		DataHelper data_helper;

		// In debug, there is no buffer data re-use optimization
		if (debug) {
			for (BufferSpec &buffer_spec : _program.buffer_specs) {
				if (!buffer_spec.is_binding) {
					buffer_spec.data_index = data_helper.allocate(buffer_spec.users_count, true);
					buffer_spec.has_data = true;
				}
			}

		} else {
			Span<BufferSpec> buffer_specs = to_span(_program.buffer_specs);

			// Allocate unique buffers (this is used notably for compile-time constants requiring a buffer)
			for (BufferSpec &buffer_spec : _program.buffer_specs) {
				if (!buffer_spec.is_binding && buffer_spec.is_pinned) {
					buffer_spec.data_index = data_helper.allocate(1, true);
					buffer_spec.has_data = true;
				}
			}

			// Allocate re-usable buffers.
			// Run through every node in execution order, allocating buffers when they are needed using pooling logic,
			// so we can precompute which buffers will actually be needed in total, ahead of running the generator.
			// This results in less memory usage than giving every buffer unique data.
			for (unsigned int order_index = 0; order_index < order.size(); ++order_index) {
				const uint32_t node_id = order[order_index];
				const ProgramGraph::Node &node = graph.get_node(node_id);
				const VoxelGraphNodeDB::NodeType &type = type_db.get_type(node.type_id);

				// Allocate data to store outputs.
				// Note, we don't allocate for inputs. The only way to allocate them is to pin them.
				for (unsigned int output_index = 0; output_index < type.outputs.size(); ++output_index) {
					const ProgramGraph::PortLocation dst_port{ node_id, output_index };
					auto address_it = _program.output_port_addresses.find(dst_port);
					ZN_ASSERT(address_it != _program.output_port_addresses.end());
					BufferSpec &buffer_spec = buffer_specs[address_it->second];

					if (buffer_spec.is_binding || buffer_spec.is_pinned) {
						continue;
					}
					buffer_spec.data_index = data_helper.allocate(buffer_spec.users_count, false);
					buffer_spec.has_data = true;
				}

				// Release references on input datas, so they can be re-used by later operations
				for (const ProgramGraph::Port &input : node.inputs) {
					if (input.connections.size() == 0) {
						continue;
					}
					const ProgramGraph::PortLocation src_port = input.connections[0];
					auto address_it = _program.output_port_addresses.find(src_port);
					ZN_ASSERT(address_it != _program.output_port_addresses.end());
					const BufferSpec &buffer_spec = buffer_specs[address_it->second];

					// Bindings are user-provided.
					// Pinned buffers are never re-used.
					if (buffer_spec.is_binding || buffer_spec.is_pinned) {
						continue;
					}

					data_helper.unref(buffer_spec.data_index);
				}
			}
		}

		_program.buffer_data_count = data_helper.datas.size();
	}

	ZN_PRINT_VERBOSE(format("Compiled voxel graph. Program size: {}b, ports: {}, buffers: {}",
			_program.operations.size() * sizeof(uint16_t), _program.buffer_count, _program.buffer_data_count));

	CompilationResult result;
	result.success = true;
	return result;
}

} // namespace zylann::voxel
