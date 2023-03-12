#include "voxel_graph_function.h"
#include "../../constants/voxel_string_names.h"
#include "../../util/container_funcs.h"
#include "../../util/godot/classes/object.h"
#include "../../util/godot/core/array.h" // for `varray` in GDExtension builds
#include "../../util/godot/core/callable.h"
#include "../../util/godot/funcs.h"
#include "../../util/string_funcs.h"
#include "node_type_db.h"

#include <algorithm>

namespace zylann::voxel::pg {

const char *VoxelGraphFunction::SIGNAL_NODE_NAME_CHANGED = "node_name_changed";

void VoxelGraphFunction::clear() {
	unregister_subresources();
	_graph.clear();
}

ProgramGraph::Node *create_node_internal(ProgramGraph &graph, VoxelGraphFunction::NodeTypeID type_id, Vector2 position,
		uint32_t id, bool create_default_instances) {
	const NodeType &type = NodeTypeDB::get_singleton().get_type(type_id);

	ProgramGraph::Node *node = graph.create_node(type_id, id);
	ERR_FAIL_COND_V(node == nullptr, nullptr);
	node->inputs.resize(type.inputs.size());
	node->outputs.resize(type.outputs.size());
	node->default_inputs.resize(type.inputs.size());
	node->gui_position = position;

	node->params.resize(type.params.size());
	for (size_t i = 0; i < type.params.size(); ++i) {
		const NodeType::Param &param = type.params[i];

		if (param.class_name.is_empty()) {
			node->params[i] = param.default_value;

		} else if (param.default_value_func != nullptr && create_default_instances) {
			node->params[i] = param.default_value_func();
		}
	}

	node->autoconnect_default_inputs = type.has_autoconnect_inputs();

	for (size_t i = 0; i < type.inputs.size(); ++i) {
		const NodeType::Port &port_spec = type.inputs[i];
		node->default_inputs[i] = port_spec.default_value;
		node->inputs[i].autoconnect_hint = port_spec.auto_connect;
	}

	return node;
}

// Automatically chooses inputs and outputs based on a graph.
void auto_pick_input_and_outputs(const ProgramGraph &graph, std::vector<VoxelGraphFunction::Port> &inputs,
		std::vector<VoxelGraphFunction::Port> &outputs) {
	const NodeTypeDB &type_db = NodeTypeDB::get_singleton();

	struct L {
		static void try_add_port(
				const VoxelGraphFunction::Port &port, std::vector<VoxelGraphFunction::Port> &added_ports) {
			for (VoxelGraphFunction::Port &p : added_ports) {
				if (p.equals(port)) {
					// Already added
					return;
				}
			}
			added_ports.push_back(port);
		}

		static void try_add_port(const ProgramGraph::Node &node, const NodeType &type,
				std::vector<VoxelGraphFunction::Port> &added_ports) {
			try_add_port(make_port_from_io_node(node, type), added_ports);
		}
	};

	struct Comparator {
		// If special input, move to top. Otherwise, use alphanumeric sorting.
		bool operator()(const VoxelGraphFunction::Port &a, const VoxelGraphFunction::Port &b) const {
			if (a.is_custom()) {
				if (b.is_custom()) {
					return a.name < b.name;
				} else {
					return false;
				}
			} else if (b.is_custom()) {
				return true;
			} else {
				if (a.type == b.type) {
					return a.sub_index < b.sub_index;
				} else {
					return a.type < b.type;
				}
			}
		}
	};

	inputs.clear();
	outputs.clear();

	graph.for_each_node_const([&type_db, &inputs, &outputs](const ProgramGraph::Node &node) {
		const NodeType &type = type_db.get_type(node.type_id);

		if (type.category == CATEGORY_INPUT) {
			L::try_add_port(node, type, inputs);

		} else if (type.category == CATEGORY_OUTPUT) {
			L::try_add_port(node, type, outputs);

		} else if (node.autoconnect_default_inputs) {
			// The input node is implicit
			for (const ProgramGraph::Port &input : node.inputs) {
				VoxelGraphFunction::NodeTypeID input_type_id;
				// If the input isn't connected and has an autoconnect hint
				if (input.connections.size() == 0 &&
						VoxelGraphFunction::try_get_node_type_id_from_auto_connect(
								VoxelGraphFunction::AutoConnect(input.autoconnect_hint), input_type_id)) {
					const NodeType &input_type = type_db.get_type(input_type_id);
					ZN_ASSERT(input_type.outputs.size() == 1);
					const VoxelGraphFunction::Port port(input_type_id, input_type.outputs[0].name);
					L::try_add_port(port, inputs);
				}
			}
		}
	});

	std::sort(inputs.begin(), inputs.end(), Comparator());
	std::sort(outputs.begin(), outputs.end(), Comparator());
}

VoxelGraphFunction::AutoConnect get_auto_connect_hint(const VoxelGraphFunction::Port &port) {
	if (!port.is_custom()) {
		VoxelGraphFunction::AutoConnect ac;
		if (VoxelGraphFunction::try_get_auto_connect_from_node_type_id(port.type, ac)) {
			return ac;
		}
	}
	return VoxelGraphFunction::AUTO_CONNECT_NONE;
}

void setup_function(ProgramGraph::Node &node, Ref<VoxelGraphFunction> func) {
	ZN_ASSERT(node.type_id == VoxelGraphFunction::NODE_FUNCTION);
	ZN_ASSERT(func.is_valid());
	ZN_ASSERT(node.params.size() >= 1);
	node.params.resize(1);
	node.params[0] = func;

	Span<const VoxelGraphFunction::Port> input_definitions = func->get_input_definitions();
	node.default_inputs.resize(input_definitions.size());
	for (Variant &di : node.default_inputs) {
		di = 0.f;
	}

	struct L {
		static void set_ports(std::vector<ProgramGraph::Port> &ports, Span<const VoxelGraphFunction::Port> fports) {
			ports.clear();
			for (const VoxelGraphFunction::Port &fport : fports) {
				ProgramGraph::Port port;
				port.autoconnect_hint = get_auto_connect_hint(fport);
				ports.push_back(port);
			}
		}
	};

	L::set_ports(node.inputs, input_definitions);
	L::set_ports(node.outputs, func->get_output_definitions());

	node.autoconnect_default_inputs = true;

	// TODO Function parameters
}

void update_function(
		ProgramGraph &graph, uint32_t node_id, std::vector<ProgramGraph::Connection> *removed_connections) {
	ProgramGraph::Node &node = graph.get_node(node_id);
	ZN_ASSERT(node.type_id == VoxelGraphFunction::NODE_FUNCTION);
	ZN_ASSERT_RETURN(node.params.size() >= 1);
	Ref<VoxelGraphFunction> func = node.params[0];
	ZN_ASSERT_RETURN(func.is_valid());

	Span<const VoxelGraphFunction::Port> input_defs = func->get_input_definitions();
	Span<const VoxelGraphFunction::Port> output_defs = func->get_output_definitions();

	ZN_ASSERT(node.default_inputs.size() == node.inputs.size());

	// Update inputs
	for (unsigned int input_index = 0; input_index < input_defs.size(); ++input_index) {
		const VoxelGraphFunction::Port &port_def = input_defs[input_index];

		if (input_index < node.inputs.size()) {
			ProgramGraph::Port &port = node.inputs[input_index];
			port.autoconnect_hint = get_auto_connect_hint(port_def);

		} else {
			ProgramGraph::Port port;
			port.autoconnect_hint = get_auto_connect_hint(port_def);
			node.inputs.push_back(port);
			node.default_inputs.push_back(0.f);
		}
	}
	// Remove excess inputs
	if (node.inputs.size() > input_defs.size()) {
		for (unsigned int i = input_defs.size(); i < node.inputs.size(); ++i) {
			const ProgramGraph::Port &port = node.inputs[i];

			if (port.connections.size() > 0) {
				ZN_ASSERT(port.connections.size() == 1);
				const ProgramGraph::PortLocation src = port.connections[0];
				const ProgramGraph::PortLocation dst{ node_id, i };
				graph.disconnect(src, dst);

				if (removed_connections != nullptr) {
					removed_connections->push_back(ProgramGraph::Connection{ src, dst });
				}
			}
		}
		node.inputs.resize(input_defs.size());
		node.default_inputs.resize(input_defs.size());
	}

	// Add new outputs
	for (unsigned int output_index = node.outputs.size(); output_index < output_defs.size(); ++output_index) {
		ProgramGraph::Port port;
		node.outputs.push_back(port);
	}
	// Remove excess outputs
	if (node.outputs.size() > output_defs.size()) {
		for (unsigned int i = output_defs.size(); i < node.outputs.size(); ++i) {
			const ProgramGraph::Port &port = node.outputs[i];
			const std::vector<ProgramGraph::PortLocation> destinations = port.connections;
			const ProgramGraph::PortLocation src{ node_id, i };

			for (const ProgramGraph::PortLocation dst : destinations) {
				graph.disconnect(src, dst);
				if (removed_connections != nullptr) {
					removed_connections->push_back(ProgramGraph::Connection{ src, dst });
				}
			}
		}
		node.outputs.resize(output_defs.size());
	}
}

uint32_t VoxelGraphFunction::create_node(NodeTypeID type_id, Vector2 position, uint32_t id) {
	ERR_FAIL_COND_V(!NodeTypeDB::get_singleton().is_valid_type_id(type_id), ProgramGraph::NULL_ID);
	ProgramGraph::Node *node = create_node_internal(_graph, type_id, position, id, true);
	ERR_FAIL_COND_V(node == nullptr, ProgramGraph::NULL_ID);
	// Register resources if any were created by default
	for (const Variant &v : node->params) {
		if (v.get_type() == Variant::OBJECT) {
			Ref<Resource> res = v;
			if (res.is_valid()) {
				register_subresource(**res);
			} else {
				ZN_PRINT_WARNING("Non-resource object found in node parameter");
			}
		}
	}
	struct L {
		static void bind_custom_port(std::vector<VoxelGraphFunction::Port> &ports, ProgramGraph::Node &node) {
			for (unsigned int port_index = 0; port_index < ports.size(); ++port_index) {
				VoxelGraphFunction::Port &port = ports[port_index];
				if (port.is_custom()) {
					// ZN_ASSERT(node->params.size() >= 1);
					// node->params[0] = input_index;
					node.name = port.name;
					// port.node_ids.push_back(node.id);
					break;
				}
			}
		}
	};
	if (type_id == NODE_CUSTOM_INPUT) {
		L::bind_custom_port(_inputs, *node);
	}
	if (type_id == NODE_CUSTOM_OUTPUT) {
		L::bind_custom_port(_outputs, *node);
	}
	return node->id;
}

uint32_t VoxelGraphFunction::create_function_node(Ref<VoxelGraphFunction> func, Vector2 position, uint32_t p_id) {
	ERR_FAIL_COND_V_MSG(func.is_null(), ProgramGraph::NULL_ID, "Cannot add null function");
	ERR_FAIL_COND_V_MSG(func.ptr() == this, ProgramGraph::NULL_ID, "Cannot add function to itself");
	ERR_FAIL_COND_V_MSG(func->contains_reference_to_function(*this), ProgramGraph::NULL_ID,
			"Cannot add function indirectly referencing itself");
	const uint32_t id = create_node(VoxelGraphFunction::NODE_FUNCTION, position, p_id);
	ERR_FAIL_COND_V(id == ProgramGraph::NULL_ID, ProgramGraph::NULL_ID);
	ProgramGraph::Node &node = _graph.get_node(id);
	setup_function(node, func);
	register_subresource(**func);
	return id;
}

void VoxelGraphFunction::remove_node(uint32_t node_id) {
	ProgramGraph::Node *node = _graph.try_get_node(node_id);
	ERR_FAIL_COND(node == nullptr);
	for (size_t i = 0; i < node->params.size(); ++i) {
		Ref<Resource> resource = node->params[i];
		if (resource.is_valid()) {
			unregister_subresource(**resource);
		}
	}
	_graph.remove_node(node_id);
	emit_changed();
}

bool VoxelGraphFunction::can_connect(
		uint32_t src_node_id, uint32_t src_port_index, uint32_t dst_node_id, uint32_t dst_port_index) const {
	const ProgramGraph::PortLocation src_port{ src_node_id, src_port_index };
	const ProgramGraph::PortLocation dst_port{ dst_node_id, dst_port_index };
	ERR_FAIL_COND_V(!_graph.is_output_port_valid(src_port), false);
	ERR_FAIL_COND_V(!_graph.is_input_port_valid(dst_port), false);
	const ProgramGraph::Node &node = _graph.get_node(src_node_id);
	// Output nodes have output ports, for internal reasons. They should not be connected.
	const NodeType &type = NodeTypeDB::get_singleton().get_type(node.type_id);
	if (type.category == CATEGORY_OUTPUT) {
		return false;
	}
	return _graph.can_connect(src_port, dst_port);
}

bool VoxelGraphFunction::is_valid_connection(
		uint32_t src_node_id, uint32_t src_port_index, uint32_t dst_node_id, uint32_t dst_port_index) const {
	const ProgramGraph::PortLocation src_port{ src_node_id, src_port_index };
	const ProgramGraph::PortLocation dst_port{ dst_node_id, dst_port_index };
	ERR_FAIL_COND_V(!_graph.is_output_port_valid(src_port), false);
	ERR_FAIL_COND_V(!_graph.is_input_port_valid(dst_port), false);
	const ProgramGraph::Node &node = _graph.get_node(src_node_id);
	const NodeType &type = NodeTypeDB::get_singleton().get_type(node.type_id);
	if (type.category == CATEGORY_OUTPUT) {
		return false;
	}
	return _graph.is_valid_connection(src_port, dst_port);
}

void VoxelGraphFunction::add_connection(
		uint32_t src_node_id, uint32_t src_port_index, uint32_t dst_node_id, uint32_t dst_port_index) {
	const ProgramGraph::PortLocation src_port{ src_node_id, src_port_index };
	const ProgramGraph::PortLocation dst_port{ dst_node_id, dst_port_index };
	ERR_FAIL_COND(!_graph.is_output_port_valid(src_port));
	ERR_FAIL_COND(!_graph.is_input_port_valid(dst_port));
	const ProgramGraph::Node &node = _graph.get_node(src_node_id);
	const NodeType &type = NodeTypeDB::get_singleton().get_type(node.type_id);
	ERR_FAIL_COND(type.category == CATEGORY_OUTPUT);
	_graph.connect(src_port, dst_port);
	emit_changed();
}

void VoxelGraphFunction::remove_connection(
		uint32_t src_node_id, uint32_t src_port_index, uint32_t dst_node_id, uint32_t dst_port_index) {
	const ProgramGraph::PortLocation src_port{ src_node_id, src_port_index };
	const ProgramGraph::PortLocation dst_port{ dst_node_id, dst_port_index };
	ERR_FAIL_COND(!_graph.is_output_port_valid(src_port));
	ERR_FAIL_COND(!_graph.is_input_port_valid(dst_port));
	_graph.disconnect(src_port, dst_port);
	emit_changed();
}

void VoxelGraphFunction::get_connections(std::vector<ProgramGraph::Connection> &connections) const {
	_graph.get_connections(connections);
}

bool VoxelGraphFunction::try_get_connection_to(
		ProgramGraph::PortLocation dst, ProgramGraph::PortLocation &out_src) const {
	const ProgramGraph::Node &node = _graph.get_node(dst.node_id);
	CRASH_COND(dst.port_index >= node.inputs.size());
	const ProgramGraph::Port &port = node.inputs[dst.port_index];
	if (port.connections.size() == 0) {
		return false;
	}
	// There can be at most one inbound connection
	out_src = port.connections[0];
	return true;
}

bool VoxelGraphFunction::has_node(uint32_t node_id) const {
	return _graph.try_get_node(node_id) != nullptr;
}

void VoxelGraphFunction::set_node_name(uint32_t node_id, StringName name) {
	ProgramGraph::Node *node = _graph.try_get_node(node_id);
	ERR_FAIL_COND_MSG(node == nullptr, "No node was found with the specified ID");
	if (node->name == name) {
		return;
	}
	if (name != StringName()) {
		const uint32_t existing_node_id = _graph.find_node_by_name(name);
		if (existing_node_id != ProgramGraph::NULL_ID && node_id == existing_node_id) {
			ZN_PRINT_ERROR(format("More than one graph node has the name \"{}\"", String(name)));
		}
	}
	node->name = name;
	emit_signal(SIGNAL_NODE_NAME_CHANGED, node_id);
}

StringName VoxelGraphFunction::get_node_name(uint32_t node_id) const {
	ProgramGraph::Node *node = _graph.try_get_node(node_id);
	ERR_FAIL_COND_V(node == nullptr, StringName());
	return node->name;
}

uint32_t VoxelGraphFunction::find_node_by_name(StringName name) const {
	return _graph.find_node_by_name(name);
}

void VoxelGraphFunction::set_node_param(uint32_t node_id, uint32_t param_index, Variant value) {
	ProgramGraph::Node *node = _graph.try_get_node(node_id);
	ERR_FAIL_COND(node == nullptr);
	ERR_FAIL_INDEX(param_index, node->params.size());

	if (node->params[param_index] != value) {
		if (node->type_id == VoxelGraphFunction::NODE_FUNCTION && param_index == 0) {
			// The function param is special, it conditions the presence of other parameters and node ports

			Ref<VoxelGraphFunction> func = value;
			ERR_FAIL_COND_MSG(func.is_null(),
					String("A Function node with a null {0} reference is not allowed")
							.format(varray(VoxelGraphFunction::get_class_static())));

			// Unregister potential resource params, since the previous function could have had different ones
			for (unsigned int i = 0; i < node->params.size(); ++i) {
				Ref<Resource> res = node->params[i];
				if (res.is_valid()) {
					unregister_subresource(**res);
				}
			}

			setup_function(*node, func);
			register_subresource(**func);

		} else {
			Ref<Resource> prev_resource = node->params[param_index];
			if (prev_resource.is_valid()) {
				unregister_subresource(**prev_resource);
			}

			node->params[param_index] = value;

			Ref<Resource> resource = value;
			if (resource.is_valid()) {
				register_subresource(**resource);
			}
		}

		emit_changed();
	}
}

bool VoxelGraphFunction::get_expression_variables(std::string_view code, std::vector<std::string_view> &vars) {
	Span<const ExpressionParser::Function> functions = NodeTypeDB::get_singleton().get_expression_parser_functions();
	ExpressionParser::Result result = ExpressionParser::parse(code, functions);
	if (result.error.id == ExpressionParser::ERROR_NONE) {
		if (result.root != nullptr) {
			ExpressionParser::find_variables(*result.root, vars);
		}
		return true;
	} else {
		return false;
	}
}

void VoxelGraphFunction::get_expression_node_inputs(uint32_t node_id, std::vector<std::string> &out_names) const {
	ProgramGraph::Node *node = _graph.try_get_node(node_id);
	ERR_FAIL_COND(node == nullptr);
	ERR_FAIL_COND(node->type_id != NODE_EXPRESSION);
	for (unsigned int i = 0; i < node->inputs.size(); ++i) {
		const ProgramGraph::Port &port = node->inputs[i];
		ERR_FAIL_COND(!port.is_dynamic());
		out_names.push_back(port.dynamic_name);
	}
}

inline bool has_duplicate(const PackedStringArray &sa) {
	return find_duplicate(Span<const String>(sa.ptr(), sa.size())) != size_t(sa.size());
}

void VoxelGraphFunction::set_expression_node_inputs(uint32_t node_id, PackedStringArray names) {
	ProgramGraph::Node *node = _graph.try_get_node(node_id);

	// Validate
	ERR_FAIL_COND(node == nullptr);
	ERR_FAIL_COND(node->type_id != NODE_EXPRESSION);
	for (int i = 0; i < names.size(); ++i) {
		const String name = names[i];
		ERR_FAIL_COND(!name.is_valid_identifier());
	}
	ERR_FAIL_COND(has_duplicate(names));
	for (unsigned int i = 0; i < node->inputs.size(); ++i) {
		const ProgramGraph::Port &port = node->inputs[i];
		// Sounds annoying if you call this from a script, but this is supposed to be editor functionality for now
		ERR_FAIL_COND_MSG(port.connections.size() > 0,
				ZN_TTR("Cannot change input ports if connections exist, disconnect them first."));
	}

	node->inputs.resize(names.size());
	node->default_inputs.resize(names.size());
	for (int i = 0; i < names.size(); ++i) {
		const String name = names[i];
		const CharString name_utf8 = name.utf8();
		node->inputs[i].dynamic_name = name_utf8.get_data();
	}
}

Variant VoxelGraphFunction::get_node_param(uint32_t node_id, uint32_t param_index) const {
	const ProgramGraph::Node *node = _graph.try_get_node(node_id);
	ERR_FAIL_COND_V(node == nullptr, Variant());
	ERR_FAIL_INDEX_V(param_index, node->params.size(), Variant());
	return node->params[param_index];
}

Variant VoxelGraphFunction::get_node_default_input(uint32_t node_id, uint32_t input_index) const {
	const ProgramGraph::Node *node = _graph.try_get_node(node_id);
	ERR_FAIL_COND_V(node == nullptr, Variant());
	ERR_FAIL_INDEX_V(input_index, node->default_inputs.size(), Variant());
	return node->default_inputs[input_index];
}

void VoxelGraphFunction::set_node_default_input(uint32_t node_id, uint32_t input_index, Variant value) {
	ProgramGraph::Node *node = _graph.try_get_node(node_id);
	ERR_FAIL_COND(node == nullptr);
	ERR_FAIL_INDEX(input_index, node->default_inputs.size());
	Variant &defval = node->default_inputs[input_index];
	if (defval != value) {
		// node->autoconnect_default_inputs = false;
		defval = value;
		emit_changed();
	}
}

bool VoxelGraphFunction::get_node_default_inputs_autoconnect(uint32_t node_id) const {
	const ProgramGraph::Node *node = _graph.try_get_node(node_id);
	ERR_FAIL_COND_V(node == nullptr, false);
	return node->autoconnect_default_inputs;
}

void VoxelGraphFunction::set_node_default_inputs_autoconnect(uint32_t node_id, bool enabled) {
	ProgramGraph::Node *node = _graph.try_get_node(node_id);
	ERR_FAIL_COND(node == nullptr);
	if (node->autoconnect_default_inputs != enabled) {
		node->autoconnect_default_inputs = enabled;
		emit_changed();
	}
}

Vector2 VoxelGraphFunction::get_node_gui_position(uint32_t node_id) const {
	const ProgramGraph::Node *node = _graph.try_get_node(node_id);
	ERR_FAIL_COND_V(node == nullptr, Vector2());
	return node->gui_position;
}

void VoxelGraphFunction::set_node_gui_position(uint32_t node_id, Vector2 pos) {
	ProgramGraph::Node *node = _graph.try_get_node(node_id);
	ERR_FAIL_COND(node == nullptr);
	if (node->gui_position != pos) {
		node->gui_position = pos;
	}
}

Vector2 VoxelGraphFunction::get_node_gui_size(uint32_t node_id) const {
	const ProgramGraph::Node *node = _graph.try_get_node(node_id);
	ERR_FAIL_COND_V(node == nullptr, Vector2());
	return node->gui_size;
}

void VoxelGraphFunction::set_node_gui_size(uint32_t node_id, Vector2 size) {
	ProgramGraph::Node *node = _graph.try_get_node(node_id);
	ERR_FAIL_COND(node == nullptr);
	if (node->gui_size != size) {
		node->gui_size = size;
	}
}

VoxelGraphFunction::NodeTypeID VoxelGraphFunction::get_node_type_id(uint32_t node_id) const {
	const ProgramGraph::Node *node = _graph.try_get_node(node_id);
	ERR_FAIL_COND_V(node == nullptr, NODE_TYPE_COUNT);
	CRASH_COND(node->type_id >= NODE_TYPE_COUNT);
	return (NodeTypeID)node->type_id;
}

PackedInt32Array VoxelGraphFunction::get_node_ids() const {
	PackedInt32Array ids;
	{
		_graph.for_each_node_id([&ids](int id) {
			// Not resizing up-front, because the code to assign elements is different between Godot modules and
			// GDExtension
			ids.append(id);
		});
	}
	return ids;
}

unsigned int VoxelGraphFunction::get_nodes_count() const {
	return _graph.get_nodes_count();
}

#ifdef TOOLS_ENABLED

void VoxelGraphFunction::get_configuration_warnings(PackedStringArray &out_warnings) const {}

uint64_t VoxelGraphFunction::get_output_graph_hash() const {
	const NodeTypeDB &type_db = NodeTypeDB::get_singleton();
	std::vector<uint32_t> terminal_nodes;

	// Not using the generic `get_terminal_nodes` function because our terminal nodes do have outputs
	_graph.for_each_node_const([&terminal_nodes, &type_db](const ProgramGraph::Node &node) {
		const NodeType &type = type_db.get_type(node.type_id);
		if (type.category == CATEGORY_OUTPUT) {
			terminal_nodes.push_back(node.id);
		}
	});

	// Sort for determinism
	std::sort(terminal_nodes.begin(), terminal_nodes.end());

	std::vector<uint32_t> order;
	_graph.find_dependencies(terminal_nodes, order);

	std::vector<uint64_t> node_hashes;
	uint64_t hash = hash_djb2_one_64(0);

	for (uint32_t node_id : order) {
		ProgramGraph::Node &node = _graph.get_node(node_id);
		hash = hash_djb2_one_64(node.type_id, hash);

		for (const Variant &v : node.params) {
			if (v.get_type() == Variant::OBJECT) {
				const Object *obj = v.operator Object *();
				if (obj != nullptr) {
					// Note, the obtained hash can change here even if the result is identical, because it's hard to
					// tell which properties contribute to the result. This should be rare though.
					hash = hash_djb2_one_64(get_deep_hash(*obj), hash);
				}
			} else {
				hash = hash_djb2_one_64(v.hash(), hash);
			}
		}

		for (const Variant &v : node.default_inputs) {
			hash = hash_djb2_one_64(v.hash(), hash);
		}

		for (const ProgramGraph::Port &port : node.inputs) {
			for (const ProgramGraph::PortLocation src : port.connections) {
				hash = hash_djb2_one_64(uint64_t(src.node_id) | (uint64_t(src.port_index) << 32), hash);
			}
		}
	}

	return hash;
}

#endif

void VoxelGraphFunction::find_dependencies(uint32_t node_id, std::vector<uint32_t> &out_dependencies) const {
	std::vector<uint32_t> dst;
	dst.push_back(node_id);
	_graph.find_dependencies(dst, out_dependencies);
}

const ProgramGraph &VoxelGraphFunction::get_graph() const {
	return _graph;
}

void VoxelGraphFunction::register_subresource(Resource &resource) {
	// print_line(String("{0}: Registering subresource {1}").format(varray(int64_t(this), int64_t(&resource))));
	resource.connect(VoxelStringNames::get_singleton().changed,
			ZN_GODOT_CALLABLE_MP(this, VoxelGraphFunction, _on_subresource_changed));
}

void VoxelGraphFunction::unregister_subresource(Resource &resource) {
	// print_line(String("{0}: Unregistering subresource {1}").format(varray(int64_t(this), int64_t(&resource))));
	resource.disconnect(VoxelStringNames::get_singleton().changed,
			ZN_GODOT_CALLABLE_MP(this, VoxelGraphFunction, _on_subresource_changed));
}

void VoxelGraphFunction::register_subresources() {
	_graph.for_each_node([this](ProgramGraph::Node &node) {
		for (size_t i = 0; i < node.params.size(); ++i) {
			Ref<Resource> resource = node.params[i];
			if (resource.is_valid()) {
				register_subresource(**resource);
			}
		}
	});
}

void VoxelGraphFunction::unregister_subresources() {
	_graph.for_each_node([this](ProgramGraph::Node &node) {
		for (size_t i = 0; i < node.params.size(); ++i) {
			Ref<Resource> resource = node.params[i];
			if (resource.is_valid()) {
				unregister_subresource(**resource);
			}
		}
	});
}

void VoxelGraphFunction::_on_subresource_changed() {
	emit_changed();
}

static Dictionary get_graph_as_variant_data(const ProgramGraph &graph) {
	/*
	{
		"version": 2,
		"nodes": {
			"1": {
				"type": "Name",
				"gui_position": Vector2(10, 10),
				"gui_size": Vector2(100, 50),
				"name": "Hello",
				"auto_connect": true,
				"input1": 0.0,
				"input2": 0.0,
				...
				"param1": 0.0,
				"param2": 0.0,
				...
				"dynamic_inputs": [
					["dyn1", 0.0],
					["dyn2", 0.0],
					...
				]
			},
			"2": {
				...
			},
			...
		},
		"connections": [
			[1, 0, 2, 0],
			[1, 0, 3, 1],
			...
		]
	}
	*/

	Dictionary nodes_data;
	graph.for_each_node_id([&graph, &nodes_data](uint32_t node_id) {
		const ProgramGraph::Node *node = graph.try_get_node(node_id);
		ERR_FAIL_COND(node == nullptr);

		Dictionary node_data;

		const NodeType &type = NodeTypeDB::get_singleton().get_type(node->type_id);
		node_data["type"] = type.name;
		node_data["gui_position"] = node->gui_position;
		if (node->gui_size != Vector2()) {
			node_data["gui_size"] = node->gui_size;
		}

		if (node->name != StringName()) {
			node_data["name"] = node->name;
		}

		// Parameters
		for (size_t j = 0; j < type.params.size(); ++j) {
			const NodeType::Param &param = type.params[j];
			node_data[param.name] = node->params[j];
		}

		if (node->autoconnect_default_inputs) {
			node_data["auto_connect"] = true;
		}

		// Static default inputs
		for (size_t j = 0; j < type.inputs.size(); ++j) {
			if (node->inputs[j].connections.size() == 0) {
				const NodeType::Port &port = type.inputs[j];
				node_data[port.name] = node->default_inputs[j];
			}
		}

		// Function default inputs
		if (node->type_id == VoxelGraphFunction::NODE_FUNCTION) {
			ZN_ASSERT(node->params.size() >= 1);
			Ref<VoxelGraphFunction> function = node->params[0];
			ZN_ASSERT_RETURN(function.is_valid());
			Span<const VoxelGraphFunction::Port> inputs = function->get_input_definitions();
			ZN_ASSERT_RETURN(inputs.size() == node->default_inputs.size());

			for (unsigned int input_index = 0; input_index < inputs.size(); ++input_index) {
				const VoxelGraphFunction::Port &input = inputs[input_index];
				node_data[input.name] = node->default_inputs[input_index];
			}
		}

		// Dynamic inputs. Order matters.
		Array dynamic_inputs_data;
		for (size_t j = 0; j < node->inputs.size(); ++j) {
			const ProgramGraph::Port &port = node->inputs[j];
			if (port.is_dynamic()) {
				Array d;
				d.resize(2);
				d[0] = String(port.dynamic_name.c_str());
				d[1] = node->default_inputs[j];
				dynamic_inputs_data.append(d);
			}
		}

		if (dynamic_inputs_data.size() > 0) {
			node_data["dynamic_inputs"] = dynamic_inputs_data;
		}

		String key = String::num_uint64(node_id);
		nodes_data[key] = node_data;
	});

	Array connections_data;
	std::vector<ProgramGraph::Connection> connections;
	graph.get_connections(connections);
	connections_data.resize(connections.size());
	for (size_t i = 0; i < connections.size(); ++i) {
		const ProgramGraph::Connection &con = connections[i];
		Array con_data;
		con_data.resize(4);
		con_data[0] = con.src.node_id;
		con_data[1] = con.src.port_index;
		con_data[2] = con.dst.node_id;
		con_data[3] = con.dst.port_index;
		connections_data[i] = con_data;
	}

	Dictionary data;
	data["nodes"] = nodes_data;
	data["connections"] = connections_data;
	data["version"] = 2;
	return data;
}

Dictionary VoxelGraphFunction::get_graph_as_variant_data() const {
	return zylann::voxel::pg::get_graph_as_variant_data(_graph);
}

static bool var_to_id(Variant v, uint32_t &out_id, uint32_t min = 0) {
	ERR_FAIL_COND_V(v.get_type() != Variant::INT, false);
	const int i = v;
	ERR_FAIL_COND_V(i < 0 || (unsigned int)i < min, false);
	out_id = i;
	return true;
}

static bool load_graph_from_variant_data(ProgramGraph &graph, Dictionary data) {
	Dictionary nodes_data = data["nodes"];
	Array connections_data = data["connections"];
	const NodeTypeDB &type_db = NodeTypeDB::get_singleton();

	// Can't iterate using `next()`, because in GDExtension, there doesn't seem to be a way to do so.
	const Array nodes_data_keys = nodes_data.keys();

	for (int nodes_data_key_index = 0; nodes_data_key_index < nodes_data_keys.size(); ++nodes_data_key_index) {
		const Variant id_key = nodes_data_keys[nodes_data_key_index];
		const String id_str = id_key;
		ERR_FAIL_COND_V(!id_str.is_valid_int(), false);
		const int sid = id_str.to_int();
		ERR_FAIL_COND_V(sid < static_cast<int>(ProgramGraph::NULL_ID), false);
		const uint32_t id = sid;

		Dictionary node_data = nodes_data[id_key];
		ERR_FAIL_COND_V(node_data.is_empty(), false);

		const String type_name = node_data["type"];
		const Vector2 gui_position = node_data["gui_position"];
		VoxelGraphFunction::NodeTypeID type_id;
		ERR_FAIL_COND_V(!type_db.try_get_type_id_from_name(type_name, type_id), false);
		// Don't create default param values, they will be assigned from serialized data
		ProgramGraph::Node *node = create_node_internal(graph, type_id, gui_position, id, false);
		ERR_FAIL_COND_V(node == nullptr, false);
		// TODO Graphs made in older versions must have autoconnect always off

		node->gui_size = node_data.get("gui_size", Vector2());

		if (type_id == VoxelGraphFunction::NODE_FUNCTION) {
			const NodeType &ntype = type_db.get_type(type_id);
			ZN_ASSERT(ntype.params.size() >= 1);
			// The function reference is always the first parameter
			const String func_key = ntype.params[0].name;
			Ref<VoxelGraphFunction> function = node_data[func_key];
			if (function.is_null()) {
				ERR_PRINT(String("Unable to load external function referenced in {0}")
								  .format(varray(VoxelGraphFunction::get_class_static())));
				continue;
			}
			setup_function(*node, function);
			// TODO Create a placeholder node in case a function isn't found to avoid loss of data?
			// For now it's probably ok as long as the user doesn't save over

			// TODO Function inputs are user-named, but they could conflict with other keys in this save format
		}

		Variant auto_connect_v = node_data.get("auto_connect", Variant());
		if (auto_connect_v != Variant()) {
			node->autoconnect_default_inputs = auto_connect_v;
		}

		// Can't iterate using `next()`, because in GDExtension, there doesn't seem to be a way to do so.
		const Array node_data_keys = node_data.keys();

		for (int node_data_key_index = 0; node_data_key_index < node_data_keys.size(); ++node_data_key_index) {
			const Variant param_key = node_data_keys[node_data_key_index];
			const String param_name = param_key;
			if (param_name == "type") {
				continue;
			}
			if (param_name == "gui_position") {
				continue;
			}
			if (param_name == "gui_size") {
				continue;
			}
			if (param_name == "auto_connect") {
				continue;
			}
			if (param_name == "dynamic_inputs") {
				const Array dynamic_inputs_data = node_data[param_key];

				for (int dpi = 0; dpi < dynamic_inputs_data.size(); ++dpi) {
					const Array d = dynamic_inputs_data[dpi];
					ERR_FAIL_COND_V(d.size() != 2, false);

					const String dynamic_param_name = d[0];
					ProgramGraph::Port dport;
					CharString dynamic_param_name_utf8 = dynamic_param_name.utf8();
					dport.dynamic_name = dynamic_param_name_utf8.get_data();

					const Variant defval = d[1];
					node->default_inputs.push_back(defval);

					node->inputs.push_back(dport);
				}

				continue;
			}
			uint32_t param_index;
			if (type_db.try_get_param_index_from_name(type_id, param_name, param_index)) {
				ERR_CONTINUE(param_index < 0 || param_index >= node->params.size());
				node->params[param_index] = node_data[param_key];
			}
			if (type_db.try_get_input_index_from_name(type_id, param_name, param_index)) {
				ERR_CONTINUE(param_index < 0 || param_index >= node->default_inputs.size());
				node->default_inputs[param_index] = node_data[param_key];
			}
			const Variant vname = node_data.get("name", Variant());
			if (vname != Variant()) {
				node->name = vname;
			}
		}
	}

	for (int i = 0; i < connections_data.size(); ++i) {
		Array con_data = connections_data[i];
		ERR_FAIL_COND_V(con_data.size() != 4, false);
		ProgramGraph::PortLocation src;
		ProgramGraph::PortLocation dst;
		ERR_FAIL_COND_V(!var_to_id(con_data[0], src.node_id, ProgramGraph::NULL_ID), false);
		ERR_FAIL_COND_V(!var_to_id(con_data[1], src.port_index), false);
		ERR_FAIL_COND_V(!var_to_id(con_data[2], dst.node_id, ProgramGraph::NULL_ID), false);
		ERR_FAIL_COND_V(!var_to_id(con_data[3], dst.port_index), false);
		// TODO Create temporary invalid connections if the node has different inputs than before (to handle the case
		// where an external function has changed)
		graph.connect(src, dst);
	}

	return true;
}

bool VoxelGraphFunction::load_graph_from_variant_data(Dictionary data) {
	clear();

	if (zylann::voxel::pg::load_graph_from_variant_data(_graph, data)) {
		register_subresources();
		return true;

	} else {
		_graph.clear();
		return false;
	}
}

void VoxelGraphFunction::get_node_input_info(
		uint32_t node_id, unsigned int input_index, String *out_name, AutoConnect *out_autoconnect) const {
	const ProgramGraph::Node &node = _graph.get_node(node_id);
	ZN_ASSERT(input_index < node.inputs.size());
	const ProgramGraph::Port &port = node.inputs[input_index];

	if (out_name != nullptr) {
		if (node.type_id == VoxelGraphFunction::NODE_FUNCTION) {
			ZN_ASSERT(node.params.size() >= 1);
			Ref<VoxelGraphFunction> func = node.params[0];

			if (func.is_valid()) {
				Span<const Port> input_defs = func->get_input_definitions();
				if (input_index < input_defs.size()) {
					*out_name = input_defs[input_index].name;
				} else {
					*out_name = "<error>";
				}

			} else {
				*out_name = "<error>";
			}

		} else {
			if (port.dynamic_name.empty()) {
				const NodeType &type = NodeTypeDB::get_singleton().get_type(node.type_id);
				ZN_ASSERT(input_index < type.inputs.size());
				*out_name = type.inputs[input_index].name;
			} else {
				*out_name = to_godot(port.dynamic_name);
			}
		}
	}

	if (out_autoconnect != nullptr) {
		*out_autoconnect = AutoConnect(port.autoconnect_hint);
	}
}

String VoxelGraphFunction::get_node_output_name(uint32_t node_id, unsigned int output_index) const {
	const ProgramGraph::Node &node = _graph.get_node(node_id);
	ZN_ASSERT(output_index < node.outputs.size());
	const ProgramGraph::Port &port = node.outputs[output_index];

	if (node.type_id == VoxelGraphFunction::NODE_FUNCTION) {
		ZN_ASSERT(node.params.size() >= 1);
		Ref<VoxelGraphFunction> func = node.params[0];

		if (func.is_valid()) {
			Span<const Port> output_defs = func->get_output_definitions();
			if (output_index < output_defs.size()) {
				return output_defs[output_index].name;
			} else {
				return "<error>";
			}

		} else {
			return "<error>";
		}

	} else {
		if (port.dynamic_name.empty()) {
			const NodeType &type = NodeTypeDB::get_singleton().get_type(node.type_id);
			ZN_ASSERT(output_index < type.outputs.size());
			return type.outputs[output_index].name;
		} else {
			return to_godot(port.dynamic_name);
		}
	}
}

unsigned int VoxelGraphFunction::get_node_input_count(uint32_t node_id) const {
	const ProgramGraph::Node *node = _graph.try_get_node(node_id);
	ERR_FAIL_COND_V(node == nullptr, 0);
	return node->inputs.size();
}

unsigned int VoxelGraphFunction::get_node_output_count(uint32_t node_id) const {
	const ProgramGraph::Node *node = _graph.try_get_node(node_id);
	ERR_FAIL_COND_V(node == nullptr, 0);
	return node->outputs.size();
}

bool VoxelGraphFunction::try_get_node_type_id_from_auto_connect(AutoConnect ac, NodeTypeID &out_node_type) {
	switch (ac) {
		case AUTO_CONNECT_X:
			out_node_type = NODE_INPUT_X;
			return true;
		case AUTO_CONNECT_Y:
			out_node_type = NODE_INPUT_Y;
			return true;
		case AUTO_CONNECT_Z:
			out_node_type = NODE_INPUT_Z;
			return true;
		case AUTO_CONNECT_NONE:
			return false;
		default:
			ZN_PRINT_ERROR(format("Unhandled auto-connect value: {0}", ac));
			return false;
			break;
	}
}

bool VoxelGraphFunction::try_get_auto_connect_from_node_type_id(NodeTypeID node_type, AutoConnect &out_ac) {
	switch (node_type) {
		case NODE_INPUT_X:
			out_ac = AUTO_CONNECT_X;
			return true;
		case NODE_INPUT_Y:
			out_ac = AUTO_CONNECT_Y;
			return true;
		case NODE_INPUT_Z:
			out_ac = AUTO_CONNECT_Z;
			return true;
		default:
			out_ac = AUTO_CONNECT_NONE;
			return false;
	}
}

Span<const VoxelGraphFunction::Port> VoxelGraphFunction::get_input_definitions() const {
	return to_span(_inputs);
}

Span<const VoxelGraphFunction::Port> VoxelGraphFunction::get_output_definitions() const {
	return to_span(_outputs);
}

bool validate_io_definitions(Span<const VoxelGraphFunction::Port> ports, const Category category) {
	const NodeTypeDB &type_db = NodeTypeDB::get_singleton();
	for (unsigned int i = 0; i < ports.size(); ++i) {
		const VoxelGraphFunction::Port &port = ports[i];
		const NodeType &type = type_db.get_type(port.type);
		if (type.category != category) {
			return false;
		}
		for (unsigned int j = i + 1; j < ports.size(); ++j) {
			const VoxelGraphFunction::Port &other_port = ports[j];
			// There must not be two ports of the same category with equivalent matching
			if (port.equals(other_port)) {
				return false;
			}
		}
	}
	return true;
}

void VoxelGraphFunction::set_io_definitions(Span<const Port> inputs, Span<const Port> outputs) {
	ERR_FAIL_COND(!validate_io_definitions(inputs, CATEGORY_INPUT));
	ERR_FAIL_COND(!validate_io_definitions(outputs, CATEGORY_OUTPUT));
	_inputs.clear();
	for (const Port &input : inputs) {
		_inputs.push_back(input);
	}
	_outputs.clear();
	for (const Port &output : outputs) {
		_outputs.push_back(output);
	}
#ifdef TOOLS_ENABLED
	notify_property_list_changed();
#endif
}

bool VoxelGraphFunction::contains_reference_to_function(Ref<VoxelGraphFunction> p_func, int max_recursion) const {
	ERR_FAIL_COND_V(p_func.is_null(), false);
	return contains_reference_to_function(**p_func, max_recursion);
}

bool VoxelGraphFunction::contains_reference_to_function(const VoxelGraphFunction &p_func, int max_recursion) const {
	ERR_FAIL_COND_V_MSG(max_recursion == -1, true,
			String("A cycle exists in a {0}, or functions are too deeply nested.")
					.format(varray(VoxelGraphFunction::get_class_static())));

	const uint32_t id = _graph.find_node([&p_func, max_recursion](const ProgramGraph::Node &node) {
		if (node.type_id == VoxelGraphFunction::NODE_FUNCTION) {
			ZN_ASSERT(node.params.size() >= 1);
			Ref<VoxelGraphFunction> func = node.params[0];
			if (func.ptr() == &p_func) {
				return true;
			}
			if (func.is_valid()) {
				return func->contains_reference_to_function(p_func, max_recursion - 1);
			}
			return false;
		} else {
			return false;
		}
	});

	return id != ProgramGraph::NULL_ID;
}

void VoxelGraphFunction::auto_pick_inputs_and_outputs() {
	zylann::voxel::pg::auto_pick_input_and_outputs(_graph, _inputs, _outputs);
}

bool find_port_by_name(Span<const VoxelGraphFunction::Port> ports, const String &name, unsigned int &out_index) {
	for (unsigned int i = 0; i < ports.size(); ++i) {
		const VoxelGraphFunction::Port &port = ports[i];
		if (port.name == name) {
			out_index = i;
			return true;
		}
	}
	return false;
}

bool VoxelGraphFunction::get_node_input_index_by_name(
		uint32_t node_id, String name, unsigned int &out_input_index) const {
	const ProgramGraph::Node &node = _graph.get_node(node_id);

	if (node.type_id == VoxelGraphFunction::NODE_FUNCTION) {
		ZN_ASSERT(node.params.size() >= 1);
		Ref<VoxelGraphFunction> func = node.params[0];
		if (func.is_valid()) {
			return find_port_by_name(func->get_input_definitions(), name, out_input_index);
		}

	} else {
		for (unsigned int i = 0; i < node.inputs.size(); ++i) {
			const std::string &dynamic_name = node.inputs[i].dynamic_name;
			if (!dynamic_name.empty() && to_godot(dynamic_name) == name) {
				out_input_index = i;
				return true;
			}
		}

		const NodeTypeDB &type_db = NodeTypeDB::get_singleton();
		return type_db.try_get_input_index_from_name(node.type_id, name, out_input_index);
	}

	return false;
}

bool VoxelGraphFunction::get_node_param_index_by_name(
		uint32_t node_id, String name, unsigned int &out_param_index) const {
	const ProgramGraph::Node &node = _graph.get_node(node_id);
	const NodeTypeDB &type_db = NodeTypeDB::get_singleton();
	return type_db.try_get_param_index_from_name(node.type_id, name, out_param_index);
}

void VoxelGraphFunction::update_function_nodes(std::vector<ProgramGraph::Connection> *removed_connections) {
	ProgramGraph &graph = _graph;
	_graph.for_each_node([&graph, removed_connections](const ProgramGraph::Node &node) {
		if (node.type_id == VoxelGraphFunction::NODE_FUNCTION) {
			update_function(graph, node.id, removed_connections);
		}
	});
}

// Binding land

int VoxelGraphFunction::_b_get_node_type_count() const {
	return NodeTypeDB::get_singleton().get_type_count();
}

Dictionary VoxelGraphFunction::_b_get_node_type_info(int type_id) const {
	ERR_FAIL_COND_V(!NodeTypeDB::get_singleton().is_valid_type_id(type_id), Dictionary());
	return NodeTypeDB::get_singleton().get_type_info_dict(type_id);
}

Array VoxelGraphFunction::_b_get_connections() const {
	Array con_array;
	std::vector<ProgramGraph::Connection> cons;
	_graph.get_connections(cons);
	con_array.resize(cons.size());

	for (size_t i = 0; i < cons.size(); ++i) {
		const ProgramGraph::Connection &con = cons[i];
		Dictionary d;
		d["src_node_id"] = con.src.node_id;
		d["src_port_index"] = con.src.port_index;
		d["dst_node_id"] = con.dst.node_id;
		d["dst_port_index"] = con.dst.port_index;
		con_array[i] = d;
	}

	return con_array;
}

void VoxelGraphFunction::_b_set_node_param_null(int node_id, int param_index) {
	set_node_param(node_id, param_index, Variant());
}

void VoxelGraphFunction::_b_set_node_name(int node_id, String name) {
	set_node_name(node_id, name);
}

PackedInt32Array to_godot_int32_array(const std::vector<uint32_t> &vec) {
	PackedInt32Array a;
	a.resize(vec.size());
	// Using pointer access because in GDExtension builds writing into a packed array has different syntax
	int32_t *p = a.ptrw();
	for (unsigned int i = 0; i < vec.size(); ++i) {
		p[i] = vec[i];
	}
	return a;
}

Array serialize_io_definitions(Span<const VoxelGraphFunction::Port> ports) {
	const NodeTypeDB &type_db = NodeTypeDB::get_singleton();
	Array data;
	data.resize(ports.size());
	for (unsigned int i = 0; i < ports.size(); ++i) {
		const VoxelGraphFunction::Port &port = ports[i];
		Array port_data;
		port_data.resize(3);

		// Name
		port_data[0] = port.name;

		// Type as a string
		const NodeType &ntype = type_db.get_type(port.type);
		port_data[1] = ntype.name;

		// Sub-index
		port_data[2] = port.sub_index;

		data[i] = port_data;
	}
	return data;
}

void deserialize_io_definitions(std::vector<VoxelGraphFunction::Port> &ports, Array data, Category expected_category) {
	const NodeTypeDB &type_db = NodeTypeDB::get_singleton();
	ports.clear();
	for (int i = 0; i < data.size(); ++i) {
		Array port_data = data[i];

		ERR_FAIL_COND(port_data.size() < 2 || port_data.size() > 3);

		VoxelGraphFunction::Port port;

		port.name = port_data[0];

		String type_name = port_data[1];
		VoxelGraphFunction::NodeTypeID type_id;
		ERR_FAIL_COND(!type_db.try_get_type_id_from_name(type_name, type_id));
		const NodeType &ntype = type_db.get_type(type_id);
		ERR_FAIL_COND(ntype.category != expected_category);
		port.type = type_id;

		if (port_data.size() >= 3) {
			port.sub_index = port_data[2];
		}

		ports.push_back(port);

		/*PackedInt32Array a = data[2];
		// Can't check if node IDs are valid, because there is no garantee Godot will deserialize graph data before IO
		// definitions.
		for (unsigned int i = 0; i < a.size(); ++i) {
			const int node_id_v = a[i];
			ERR_FAIL_COND(node_id_v < 0);
			const uint32_t node_id = node_id_v;
			ERR_FAIL_COND(node_id == ProgramGraph::NULL_ID);
			port.node_ids.push_back(node_id);
		}*/
	}
}

Array VoxelGraphFunction::_b_get_input_definitions() const {
	return serialize_io_definitions(to_span(_inputs));
}

void VoxelGraphFunction::_b_set_input_definitions(Array data) {
	deserialize_io_definitions(_inputs, data, CATEGORY_INPUT);
#ifdef TOOLS_ENABLED
	notify_property_list_changed();
#endif
}

Array VoxelGraphFunction::_b_get_output_definitions() const {
	return serialize_io_definitions(to_span(_outputs));
}

void VoxelGraphFunction::_b_set_output_definitions(Array data) {
	deserialize_io_definitions(_outputs, data, CATEGORY_OUTPUT);
#ifdef TOOLS_ENABLED
	notify_property_list_changed();
#endif
}

void VoxelGraphFunction::_bind_methods() {
	ClassDB::bind_method(D_METHOD("clear"), &VoxelGraphFunction::clear);
	ClassDB::bind_method(D_METHOD("create_node", "type_id", "position", "id"), &VoxelGraphFunction::create_node,
			DEFVAL(ProgramGraph::NULL_ID));
	ClassDB::bind_method(D_METHOD("create_function_node", "position", "id"), &VoxelGraphFunction::create_function_node,
			DEFVAL(ProgramGraph::NULL_ID));
	ClassDB::bind_method(D_METHOD("remove_node", "node_id"), &VoxelGraphFunction::remove_node);
	ClassDB::bind_method(D_METHOD("can_connect", "src_node_id", "src_port_index", "dst_node_id", "dst_port_index"),
			&VoxelGraphFunction::can_connect);
	ClassDB::bind_method(D_METHOD("add_connection", "src_node_id", "src_port_index", "dst_node_id", "dst_port_index"),
			&VoxelGraphFunction::add_connection);
	ClassDB::bind_method(
			D_METHOD("remove_connection", "src_node_id", "src_port_index", "dst_node_id", "dst_port_index"),
			&VoxelGraphFunction::remove_connection);
	ClassDB::bind_method(D_METHOD("get_connections"), &VoxelGraphFunction::_b_get_connections);
	ClassDB::bind_method(D_METHOD("get_node_ids"), &VoxelGraphFunction::get_node_ids);
	ClassDB::bind_method(D_METHOD("find_node_by_name", "name"), &VoxelGraphFunction::find_node_by_name);

	ClassDB::bind_method(D_METHOD("get_node_type_id", "node_id"), &VoxelGraphFunction::get_node_type_id);
	ClassDB::bind_method(D_METHOD("get_node_param", "node_id", "param_index"), &VoxelGraphFunction::get_node_param);
	ClassDB::bind_method(
			D_METHOD("set_node_param", "node_id", "param_index", "value"), &VoxelGraphFunction::set_node_param);
	ClassDB::bind_method(
			D_METHOD("get_node_default_input", "node_id", "input_index"), &VoxelGraphFunction::get_node_default_input);
	ClassDB::bind_method(D_METHOD("set_node_default_input", "node_id", "input_index", "value"),
			&VoxelGraphFunction::set_node_default_input);
	ClassDB::bind_method(D_METHOD("get_node_default_inputs_autoconnect", "node_id"),
			&VoxelGraphFunction::get_node_default_inputs_autoconnect);
	ClassDB::bind_method(D_METHOD("set_node_default_inputs_autoconnect", "node_id", "enabled"),
			&VoxelGraphFunction::set_node_default_inputs_autoconnect);
	ClassDB::bind_method(
			D_METHOD("set_node_param_null", "node_id", "param_index"), &VoxelGraphFunction::_b_set_node_param_null);
	ClassDB::bind_method(D_METHOD("get_node_gui_position", "node_id"), &VoxelGraphFunction::get_node_gui_position);
	ClassDB::bind_method(
			D_METHOD("set_node_gui_position", "node_id", "position"), &VoxelGraphFunction::set_node_gui_position);
	ClassDB::bind_method(D_METHOD("get_node_gui_size", "node_id"), &VoxelGraphFunction::get_node_gui_size);
	ClassDB::bind_method(D_METHOD("set_node_gui_size", "node_id", "size"), &VoxelGraphFunction::set_node_gui_size);
	ClassDB::bind_method(D_METHOD("get_node_name", "node_id"), &VoxelGraphFunction::get_node_name);
	ClassDB::bind_method(D_METHOD("set_node_name", "node_id", "name"), &VoxelGraphFunction::set_node_name);
	ClassDB::bind_method(D_METHOD("set_expression_node_inputs", "node_id", "names"),
			&VoxelGraphFunction::set_expression_node_inputs);

	ClassDB::bind_method(D_METHOD("get_node_type_count"), &VoxelGraphFunction::_b_get_node_type_count);
	ClassDB::bind_method(D_METHOD("get_node_type_info", "type_id"), &VoxelGraphFunction::_b_get_node_type_info);

	ClassDB::bind_method(D_METHOD("_set_graph_data", "data"), &VoxelGraphFunction::load_graph_from_variant_data);
	ClassDB::bind_method(D_METHOD("_get_graph_data"), &VoxelGraphFunction::get_graph_as_variant_data);

	ClassDB::bind_method(D_METHOD("_set_input_definitions", "data"), &VoxelGraphFunction::_b_set_input_definitions);
	ClassDB::bind_method(D_METHOD("_get_input_definitions"), &VoxelGraphFunction::_b_get_input_definitions);

	ClassDB::bind_method(D_METHOD("_set_output_definitions", "data"), &VoxelGraphFunction::_b_set_output_definitions);
	ClassDB::bind_method(D_METHOD("_get_output_definitions"), &VoxelGraphFunction::_b_get_output_definitions);

#ifdef ZN_GODOT_EXTENSION
	ClassDB::bind_method(D_METHOD("_on_subresource_changed"), &VoxelGraphFunction::_on_subresource_changed);
#endif

	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "graph_data", PROPERTY_HINT_NONE, "",
						 PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL),
			"_set_graph_data", "_get_graph_data");

	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "input_definitions"), "_set_input_definitions", "_get_input_definitions");

	ADD_PROPERTY(
			PropertyInfo(Variant::ARRAY, "output_definitions"), "_set_output_definitions", "_get_output_definitions");

	ADD_SIGNAL(MethodInfo(SIGNAL_NODE_NAME_CHANGED, PropertyInfo(Variant::INT, "node_id")));

	BIND_ENUM_CONSTANT(NODE_CONSTANT);
	BIND_ENUM_CONSTANT(NODE_INPUT_X);
	BIND_ENUM_CONSTANT(NODE_INPUT_Y);
	BIND_ENUM_CONSTANT(NODE_INPUT_Z);
	BIND_ENUM_CONSTANT(NODE_OUTPUT_SDF);
	BIND_ENUM_CONSTANT(NODE_CUSTOM_INPUT);
	BIND_ENUM_CONSTANT(NODE_CUSTOM_OUTPUT);
	BIND_ENUM_CONSTANT(NODE_ADD);
	BIND_ENUM_CONSTANT(NODE_SUBTRACT);
	BIND_ENUM_CONSTANT(NODE_MULTIPLY);
	BIND_ENUM_CONSTANT(NODE_DIVIDE);
	BIND_ENUM_CONSTANT(NODE_SIN);
	BIND_ENUM_CONSTANT(NODE_FLOOR);
	BIND_ENUM_CONSTANT(NODE_ABS);
	BIND_ENUM_CONSTANT(NODE_SQRT);
	BIND_ENUM_CONSTANT(NODE_FRACT);
	BIND_ENUM_CONSTANT(NODE_STEPIFY);
	BIND_ENUM_CONSTANT(NODE_WRAP);
	BIND_ENUM_CONSTANT(NODE_MIN);
	BIND_ENUM_CONSTANT(NODE_MAX);
	BIND_ENUM_CONSTANT(NODE_DISTANCE_2D);
	BIND_ENUM_CONSTANT(NODE_DISTANCE_3D);
	BIND_ENUM_CONSTANT(NODE_CLAMP);
	BIND_ENUM_CONSTANT(NODE_MIX);
	BIND_ENUM_CONSTANT(NODE_REMAP);
	BIND_ENUM_CONSTANT(NODE_SMOOTHSTEP);
	BIND_ENUM_CONSTANT(NODE_CURVE);
	BIND_ENUM_CONSTANT(NODE_SELECT);
	BIND_ENUM_CONSTANT(NODE_NOISE_2D);
	BIND_ENUM_CONSTANT(NODE_NOISE_3D);
	BIND_ENUM_CONSTANT(NODE_IMAGE_2D);
	BIND_ENUM_CONSTANT(NODE_SDF_PLANE);
	BIND_ENUM_CONSTANT(NODE_SDF_BOX);
	BIND_ENUM_CONSTANT(NODE_SDF_SPHERE);
	BIND_ENUM_CONSTANT(NODE_SDF_TORUS);
	BIND_ENUM_CONSTANT(NODE_SDF_PREVIEW);
	BIND_ENUM_CONSTANT(NODE_SDF_SPHERE_HEIGHTMAP);
	BIND_ENUM_CONSTANT(NODE_SDF_SMOOTH_UNION);
	BIND_ENUM_CONSTANT(NODE_SDF_SMOOTH_SUBTRACT);
	BIND_ENUM_CONSTANT(NODE_NORMALIZE_3D);
	BIND_ENUM_CONSTANT(NODE_FAST_NOISE_2D);
	BIND_ENUM_CONSTANT(NODE_FAST_NOISE_3D);
	BIND_ENUM_CONSTANT(NODE_FAST_NOISE_GRADIENT_2D);
	BIND_ENUM_CONSTANT(NODE_FAST_NOISE_GRADIENT_3D);
	BIND_ENUM_CONSTANT(NODE_OUTPUT_WEIGHT);
#ifdef VOXEL_ENABLE_FAST_NOISE_2
	BIND_ENUM_CONSTANT(NODE_FAST_NOISE_2_2D);
	BIND_ENUM_CONSTANT(NODE_FAST_NOISE_2_3D);
#endif
	BIND_ENUM_CONSTANT(NODE_OUTPUT_SINGLE_TEXTURE);
	BIND_ENUM_CONSTANT(NODE_EXPRESSION);
	BIND_ENUM_CONSTANT(NODE_POWI);
	BIND_ENUM_CONSTANT(NODE_POW);
	BIND_ENUM_CONSTANT(NODE_INPUT_SDF);
	BIND_ENUM_CONSTANT(NODE_COMMENT);
	BIND_ENUM_CONSTANT(NODE_FUNCTION);
	BIND_ENUM_CONSTANT(NODE_RELAY);
	BIND_ENUM_CONSTANT(NODE_SPOTS_2D);
	BIND_ENUM_CONSTANT(NODE_SPOTS_3D);
	BIND_ENUM_CONSTANT(NODE_TYPE_COUNT);
}

} // namespace zylann::voxel::pg
