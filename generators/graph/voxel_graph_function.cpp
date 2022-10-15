#include "voxel_graph_function.h"
#include "../../constants/voxel_string_names.h"
#include "../../util/container_funcs.h"
#include "../../util/godot/callable.h"
#include "../../util/godot/object.h"
#include "../../util/string_funcs.h"
#include "voxel_graph_node_db.h"
#include <algorithm>

namespace zylann::voxel {

const char *VoxelGraphFunction::SIGNAL_NODE_NAME_CHANGED = "node_name_changed";

void VoxelGraphFunction::clear() {
	unregister_subresources();
	_graph.clear();
}

ProgramGraph::Node *create_node_internal(ProgramGraph &graph, VoxelGraphFunction::NodeTypeID type_id, Vector2 position,
		uint32_t id, bool create_default_instances) {
	const VoxelGraphNodeDB::NodeType &type = VoxelGraphNodeDB::get_singleton().get_type(type_id);

	ProgramGraph::Node *node = graph.create_node(type_id, id);
	ERR_FAIL_COND_V(node == nullptr, nullptr);
	node->inputs.resize(type.inputs.size());
	node->outputs.resize(type.outputs.size());
	node->default_inputs.resize(type.inputs.size());
	node->gui_position = position;

	node->params.resize(type.params.size());
	for (size_t i = 0; i < type.params.size(); ++i) {
		const VoxelGraphNodeDB::Param &param = type.params[i];

		if (param.class_name.is_empty()) {
			node->params[i] = param.default_value;

		} else if (param.default_value_func != nullptr && create_default_instances) {
			node->params[i] = param.default_value_func();
		}
	}

	node->autoconnect_default_inputs = type.has_autoconnect_inputs();

	for (size_t i = 0; i < type.inputs.size(); ++i) {
		node->default_inputs[i] = type.inputs[i].default_value;
	}

	return node;
}

uint32_t VoxelGraphFunction::create_node(NodeTypeID type_id, Vector2 position, uint32_t id) {
	ERR_FAIL_COND_V(!VoxelGraphNodeDB::get_singleton().is_valid_type_id(type_id), ProgramGraph::NULL_ID);
	const ProgramGraph::Node *node = create_node_internal(_graph, type_id, position, id, true);
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
	return node->id;
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
	return _graph.can_connect(src_port, dst_port);
}

bool VoxelGraphFunction::is_valid_connection(
		uint32_t src_node_id, uint32_t src_port_index, uint32_t dst_node_id, uint32_t dst_port_index) const {
	const ProgramGraph::PortLocation src_port{ src_node_id, src_port_index };
	const ProgramGraph::PortLocation dst_port{ dst_node_id, dst_port_index };
	ERR_FAIL_COND_V(!_graph.is_output_port_valid(src_port), false);
	ERR_FAIL_COND_V(!_graph.is_input_port_valid(dst_port), false);
	return _graph.is_valid_connection(src_port, dst_port);
}

void VoxelGraphFunction::add_connection(
		uint32_t src_node_id, uint32_t src_port_index, uint32_t dst_node_id, uint32_t dst_port_index) {
	const ProgramGraph::PortLocation src_port{ src_node_id, src_port_index };
	const ProgramGraph::PortLocation dst_port{ dst_node_id, dst_port_index };
	ERR_FAIL_COND(!_graph.is_output_port_valid(src_port));
	ERR_FAIL_COND(!_graph.is_input_port_valid(dst_port));
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
			ZN_PRINT_ERROR(format("More than one graph node has the name \"{}\"", GodotStringWrapper(String(name))));
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
		Ref<Resource> prev_resource = node->params[param_index];
		if (prev_resource.is_valid()) {
			unregister_subresource(**prev_resource);
		}

		node->params[param_index] = value;

		Ref<Resource> resource = value;
		if (resource.is_valid()) {
			register_subresource(**resource);
		}

		emit_changed();
	}
}

bool VoxelGraphFunction::get_expression_variables(std::string_view code, std::vector<std::string_view> &vars) {
	Span<const ExpressionParser::Function> functions =
			VoxelGraphNodeDB::get_singleton().get_expression_parser_functions();
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
		//node->autoconnect_default_inputs = false;
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
	const VoxelGraphNodeDB &type_db = VoxelGraphNodeDB::get_singleton();
	std::vector<uint32_t> terminal_nodes;

	// Not using the generic `get_terminal_nodes` function because our terminal nodes do have outputs
	_graph.for_each_node_const([&terminal_nodes, &type_db](const ProgramGraph::Node &node) {
		const VoxelGraphNodeDB::NodeType &type = type_db.get_type(node.type_id);
		if (type.category == VoxelGraphNodeDB::CATEGORY_OUTPUT) {
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
	//print_line(String("{0}: Registering subresource {1}").format(varray(int64_t(this), int64_t(&resource))));
	resource.connect(VoxelStringNames::get_singleton().changed,
			ZN_GODOT_CALLABLE_MP(this, VoxelGraphFunction, _on_subresource_changed));
}

void VoxelGraphFunction::unregister_subresource(Resource &resource) {
	//print_line(String("{0}: Unregistering subresource {1}").format(varray(int64_t(this), int64_t(&resource))));
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
	Dictionary nodes_data;
	graph.for_each_node_id([&graph, &nodes_data](uint32_t node_id) {
		const ProgramGraph::Node *node = graph.try_get_node(node_id);
		ERR_FAIL_COND(node == nullptr);

		Dictionary node_data;

		const VoxelGraphNodeDB::NodeType &type = VoxelGraphNodeDB::get_singleton().get_type(node->type_id);
		node_data["type"] = type.name;
		node_data["gui_position"] = node->gui_position;
		if (node->gui_size != Vector2()) {
			node_data["gui_size"] = node->gui_size;
		}

		if (node->name != StringName()) {
			node_data["name"] = node->name;
		}

		for (size_t j = 0; j < type.params.size(); ++j) {
			const VoxelGraphNodeDB::Param &param = type.params[j];
			node_data[param.name] = node->params[j];
		}

		// Static inputs
		for (size_t j = 0; j < type.inputs.size(); ++j) {
			if (node->inputs[j].connections.size() == 0) {
				const VoxelGraphNodeDB::Port &port = type.inputs[j];
				node_data[port.name] = node->default_inputs[j];
			}
		}

		if (node->autoconnect_default_inputs) {
			node_data["auto_connect"] = true;
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
	return zylann::voxel::get_graph_as_variant_data(_graph);
}

static bool var_to_id(Variant v, uint32_t &out_id, uint32_t min = 0) {
	ERR_FAIL_COND_V(v.get_type() != Variant::INT, false);
	int i = v;
	ERR_FAIL_COND_V(i < 0 || (unsigned int)i < min, false);
	out_id = i;
	return true;
}

static bool load_graph_from_variant_data(ProgramGraph &graph, Dictionary data) {
	Dictionary nodes_data = data["nodes"];
	Array connections_data = data["connections"];
	const VoxelGraphNodeDB &type_db = VoxelGraphNodeDB::get_singleton();

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
		graph.connect(src, dst);
	}

	return true;
}

bool VoxelGraphFunction::load_graph_from_variant_data(Dictionary data) {
	clear();

	if (zylann::voxel::load_graph_from_variant_data(_graph, data)) {
		register_subresources();
		return true;

	} else {
		_graph.clear();
		return false;
	}
}

// Binding land

int VoxelGraphFunction::_b_get_node_type_count() const {
	return VoxelGraphNodeDB::get_singleton().get_type_count();
}

Dictionary VoxelGraphFunction::_b_get_node_type_info(int type_id) const {
	ERR_FAIL_COND_V(!VoxelGraphNodeDB::get_singleton().is_valid_type_id(type_id), Dictionary());
	return VoxelGraphNodeDB::get_singleton().get_type_info_dict(type_id);
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

void VoxelGraphFunction::_bind_methods() {
	ClassDB::bind_method(D_METHOD("clear"), &VoxelGraphFunction::clear);
	ClassDB::bind_method(D_METHOD("create_node", "type_id", "position", "id"), &VoxelGraphFunction::create_node,
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

#ifdef ZN_GODOT_EXTENSION
	ClassDB::bind_method(D_METHOD("_on_subresource_changed"), &VoxelGraphFunction::_on_subresource_changed);
#endif

	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "graph_data", PROPERTY_HINT_NONE, "",
						 PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL),
			"_set_graph_data", "_get_graph_data");

	ADD_SIGNAL(MethodInfo(SIGNAL_NODE_NAME_CHANGED, PropertyInfo(Variant::INT, "node_id")));

	BIND_ENUM_CONSTANT(NODE_CONSTANT);
	BIND_ENUM_CONSTANT(NODE_INPUT_X);
	BIND_ENUM_CONSTANT(NODE_INPUT_Y);
	BIND_ENUM_CONSTANT(NODE_INPUT_Z);
	BIND_ENUM_CONSTANT(NODE_OUTPUT_SDF);
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
	BIND_ENUM_CONSTANT(NODE_TYPE_COUNT);
}

} // namespace zylann::voxel
