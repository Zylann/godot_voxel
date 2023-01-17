#include "voxel_graph_node_inspector_wrapper.h"
#include "../../generators/graph/node_type_db.h"
#include "../../util/godot/core/array.h"
#include "../../util/godot/funcs.h"
#include "../../util/log.h"
#include "voxel_graph_editor.h"

#include <algorithm>

namespace zylann::voxel {

using namespace pg;

namespace {
const char *AUTOCONNECT_PROPERTY_NAME = "autoconnect_default_inputs";
}

void VoxelGraphNodeInspectorWrapper::setup(uint32_t p_node_id, VoxelGraphEditor *ed) {
	ZN_ASSERT(ed != nullptr);
	_graph = ed->get_graph();
	_generator = ed->get_generator();
	_node_id = p_node_id;
	_graph_editor = ed;
}

void VoxelGraphNodeInspectorWrapper::detach_from_graph_editor() {
	_graph = Ref<VoxelGraphFunction>();
	_generator = Ref<VoxelGeneratorGraph>();
	_graph_editor = nullptr;
}

void VoxelGraphNodeInspectorWrapper::_get_property_list(List<PropertyInfo> *p_list) const {
	Ref<VoxelGraphFunction> graph = get_graph();
	ERR_FAIL_COND(graph.is_null());

	if (!graph->has_node(_node_id)) {
		// Maybe got erased by the user?
#ifdef DEBUG_ENABLED
		ZN_PRINT_VERBOSE("VoxelGeneratorGraph node was not found, from the graph inspector");
#endif
		return;
	}

	p_list->push_back(PropertyInfo(Variant::STRING_NAME, "name", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR));

	// Params
	{
		const uint32_t node_type_id = graph->get_node_type_id(_node_id);
		const NodeType &node_type = NodeTypeDB::get_singleton().get_type(node_type_id);

		p_list->push_back(PropertyInfo(Variant::NIL, "Params", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_CATEGORY));

		for (const NodeType::Param &param : node_type.params) {
			if (param.hidden) {
				continue;
			}
			PropertyInfo pi;
			pi.name = param.name;
			pi.type = param.type;
			pi.class_name = param.class_name;

			if (!param.class_name.is_empty()) {
				pi.hint = PROPERTY_HINT_RESOURCE_TYPE;
				pi.hint_string = pi.class_name;

			} else if (param.has_range) {
				pi.hint = PROPERTY_HINT_RANGE;
				pi.hint_string = String("{0},{1}").format(varray(param.min_value, param.max_value));

			} else if (pi.type == Variant::STRING) {
				if (param.multiline) {
					pi.hint = PROPERTY_HINT_MULTILINE_TEXT;
				}

			} else if (param.enum_items.size() > 0) {
				std::string hint_string;
				for (unsigned int item_index = 0; item_index < param.enum_items.size(); ++item_index) {
					if (item_index > 0) {
						hint_string += ",";
					}
					hint_string += param.enum_items[item_index];
				}
				pi.hint_string = to_godot(hint_string);
				pi.hint = PROPERTY_HINT_ENUM;
			}

			pi.usage = PROPERTY_USAGE_EDITOR;
			p_list->push_back(pi);
		}
	}

	// Inputs

	p_list->push_back(PropertyInfo(Variant::NIL, "Input Defaults", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_CATEGORY));

	const bool autoconnect_enabled = graph->get_node_default_inputs_autoconnect(_node_id);
	const unsigned int input_count = graph->get_node_input_count(_node_id);
	bool has_autoconnect_inputs = false;

	for (unsigned int i = 0; i < input_count; ++i) {
		String port_name;
		VoxelGraphFunction::AutoConnect autoconnect_hint;
		graph->get_node_input_info(_node_id, i, &port_name, &autoconnect_hint);

		if (autoconnect_hint != VoxelGraphFunction::AUTO_CONNECT_NONE) {
			has_autoconnect_inputs = true;
		}

		PropertyInfo pi;
		pi.name = port_name;
		// All I/Os are floats at the moment.
		pi.type = Variant::FLOAT;
		if (autoconnect_enabled && autoconnect_hint != VoxelGraphFunction::AUTO_CONNECT_NONE) {
			// This default value won't be used because the port will automatically connect when compiled
			pi.usage |= PROPERTY_USAGE_READ_ONLY;
		}
		p_list->push_back(pi);
	}

	// Autoconnect

	if (has_autoconnect_inputs) {
		p_list->push_back(PropertyInfo(Variant::BOOL, AUTOCONNECT_PROPERTY_NAME));
	}
}

// Automatically updates the list of inputs from variable names used in the expression.
// Contrary to VisualScript (for which this has to be done manually to the user), submitting the text field containing
// the expression's code also changes dynamic inputs of the node and reconnects existing connections, all as one
// UndoRedo action.
static void update_expression_inputs(VoxelGraphFunction &graph, uint32_t node_id, String code,
		EditorUndoRedoManager &ur, VoxelGraphEditor &graph_editor) {
	//
	const CharString code_utf8 = code.utf8();
	std::vector<std::string_view> new_input_names;
	if (!VoxelGraphFunction::get_expression_variables(code_utf8.get_data(), new_input_names)) {
		// Error, the action will not include node input changes
		return;
	}
	std::vector<std::string> old_input_names;
	graph.get_expression_node_inputs(node_id, old_input_names);

	struct Connection {
		ProgramGraph::PortLocation src;
		uint32_t dst_port_index;
	};
	// Find what we'll disconnect
	std::vector<Connection> to_disconnect;
	for (uint32_t port_index = 0; port_index < old_input_names.size(); ++port_index) {
		ProgramGraph::PortLocation src;
		if (graph.try_get_connection_to({ node_id, port_index }, src)) {
			to_disconnect.push_back({ { src.node_id, src.port_index }, port_index });
		}
	}
	// Find what we'll reconnect
	std::vector<Connection> to_reconnect;
	for (uint32_t port_index = 0; port_index < old_input_names.size(); ++port_index) {
		const std::string_view old_name = old_input_names[port_index];
		auto new_input_name_it = std::find(new_input_names.begin(), new_input_names.end(), old_name);
		if (new_input_name_it != new_input_names.end()) {
			ProgramGraph::PortLocation src;
			if (graph.try_get_connection_to({ node_id, port_index }, src)) {
				const uint32_t dst_port_index = new_input_name_it - new_input_names.begin();
				to_reconnect.push_back({ src, dst_port_index });
			}
		}
	}

	// Do

	for (size_t i = 0; i < to_disconnect.size(); ++i) {
		const Connection con = to_disconnect[i];
		ur.add_do_method(&graph, "remove_connection", con.src.node_id, con.src.port_index, node_id, con.dst_port_index);
	}

	ur.add_do_method(&graph, "set_expression_node_inputs", node_id, to_godot(new_input_names));

	for (size_t i = 0; i < to_reconnect.size(); ++i) {
		const Connection con = to_reconnect[i];
		ur.add_do_method(&graph, "add_connection", con.src.node_id, con.src.port_index, node_id, con.dst_port_index);
	}

	// Undo

	for (size_t i = 0; i < to_reconnect.size(); ++i) {
		const Connection con = to_reconnect[i];
		ur.add_undo_method(
				&graph, "remove_connection", con.src.node_id, con.src.port_index, node_id, con.dst_port_index);
	}

	ur.add_undo_method(&graph, "set_expression_node_inputs", node_id, to_godot(old_input_names));

	for (size_t i = 0; i < to_disconnect.size(); ++i) {
		const Connection con = to_disconnect[i];
		ur.add_undo_method(&graph, "add_connection", con.src.node_id, con.src.port_index, node_id, con.dst_port_index);
	}

	ur.add_do_method(&graph_editor, "update_node_layout", node_id);
	ur.add_undo_method(&graph_editor, "update_node_layout", node_id);
}

bool VoxelGraphNodeInspectorWrapper::_set(const StringName &p_name, const Variant &p_value) {
	Ref<VoxelGraphFunction> graph = get_graph();
	ERR_FAIL_COND_V(graph.is_null(), false);
	ERR_FAIL_COND_V(_graph_editor == nullptr, false);
	// We cannot keep a reference to UndoRedo in our object because our object can be referenced by UndoRedo, which
	// would cause a cyclic reference. So we access it from a weak reference to the editor.
	EditorUndoRedoManager *undo_redo = _graph_editor->get_undo_redo();
	ERR_FAIL_COND_V(undo_redo == nullptr, false);
	EditorUndoRedoManager &ur = *undo_redo;

	const String name = p_name;

	// Special case because `name` is neither a parameter nor an output
	if (name == "name") {
		String previous_name = graph->get_node_name(_node_id);
		ur.create_action("Set VoxelGeneratorGraph node name");
		ur.add_do_method(graph.ptr(), "set_node_name", _node_id, p_value);
		ur.add_undo_method(graph.ptr(), "set_node_name", _node_id, previous_name);
		// ur->add_do_method(this, "notify_property_list_changed");
		// ur->add_undo_method(this, "notify_property_list_changed");
		ur.commit_action();
		return true;
	}

	if (name == AUTOCONNECT_PROPERTY_NAME) {
		ur.create_action(String("Set ") + AUTOCONNECT_PROPERTY_NAME);
		const bool prev_autoconnect = graph->get_node_default_inputs_autoconnect(_node_id);
		ur.add_do_method(graph.ptr(), "set_node_default_inputs_autoconnect", _node_id, p_value);
		ur.add_undo_method(graph.ptr(), "set_node_default_inputs_autoconnect", _node_id, prev_autoconnect);
		// To update disabled default input values in the inspector
		ur.add_do_method(this, "notify_property_list_changed");
		ur.add_undo_method(this, "notify_property_list_changed");
		ur.commit_action();
		return true;
	}

	uint32_t index;

	if (graph->get_node_param_index_by_name(_node_id, p_name, index)) {
		Variant previous_value = graph->get_node_param(_node_id, index);
		ur.create_action("Set VoxelGeneratorGraph node parameter");
		ur.add_do_method(graph.ptr(), "set_node_param", _node_id, index, p_value);
		ur.add_undo_method(graph.ptr(), "set_node_param", _node_id, index, previous_value);

		const VoxelGraphFunction::NodeTypeID node_type_id = _graph->get_node_type_id(_node_id);

		if (node_type_id == VoxelGraphFunction::NODE_EXPRESSION) {
			update_expression_inputs(**graph, _node_id, p_value, ur, *_graph_editor);
			// TODO Default inputs cannot be set after adding variables!
			// It requires calling `notify_property_list_changed`, however that makes the LineEdit in the inspector to
			// reset its cursor position, making string parameter edition a nightmare. Only workaround is to deselect
			// and re-select the node...
		} else if (node_type_id == VoxelGraphFunction::NODE_COMMENT) {
			ur.add_do_method(_graph_editor, "update_node_comment", _node_id);
			ur.add_undo_method(_graph_editor, "update_node_comment", _node_id);
		} else {
			ur.add_do_method(this, "notify_property_list_changed");
			ur.add_undo_method(this, "notify_property_list_changed");
		}

		ur.commit_action();

	} else if (graph->get_node_input_index_by_name(_node_id, p_name, index)) {
		Variant previous_value = graph->get_node_default_input(_node_id, index);
		ur.create_action("Set VoxelGeneratorGraph node default input");
		ur.add_do_method(graph.ptr(), "set_node_default_input", _node_id, index, p_value);
		ur.add_undo_method(graph.ptr(), "set_node_default_input", _node_id, index, previous_value);
		ur.add_do_method(this, "notify_property_list_changed");
		ur.add_undo_method(this, "notify_property_list_changed");
		ur.commit_action();

	} else {
		ERR_PRINT(String("Invalid param name {0}").format(varray(p_name)));
		return false;
	}

	return true;
}

bool VoxelGraphNodeInspectorWrapper::_get(const StringName &p_name, Variant &r_ret) const {
	Ref<VoxelGraphFunction> graph = get_graph();
	ERR_FAIL_COND_V(graph.is_null(), false);

	const String name = p_name;

	if (name == "name") {
		r_ret = graph->get_node_name(_node_id);
		return true;
	}

	if (name == AUTOCONNECT_PROPERTY_NAME) {
		r_ret = graph->get_node_default_inputs_autoconnect(_node_id);
		return true;
	}

	unsigned int index;
	if (graph->get_node_param_index_by_name(_node_id, p_name, index)) {
		r_ret = graph->get_node_param(_node_id, index);

	} else if (graph->get_node_input_index_by_name(_node_id, p_name, index)) {
		r_ret = graph->get_node_default_input(_node_id, index);

	} else {
		ERR_PRINT(String("Invalid param name {0}").format(varray(p_name)));
		return false;
	}

	return true;
}

// This method is an undocumented hack used in `EditorInspector::_edit_set` so we can implement UndoRedo ourselves.
// If we don't do this, then the inspector's UndoRedo will use the wrapper, which won't mark the real resource as
// modified.
bool VoxelGraphNodeInspectorWrapper::_dont_undo_redo() const {
	return true;
}

void VoxelGraphNodeInspectorWrapper::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_dont_undo_redo"), &VoxelGraphNodeInspectorWrapper::_dont_undo_redo);
}

} // namespace zylann::voxel
