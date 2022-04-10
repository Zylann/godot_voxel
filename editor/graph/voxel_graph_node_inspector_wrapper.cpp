#include "voxel_graph_node_inspector_wrapper.h"
#include "../../generators/graph/voxel_graph_node_db.h"
#include "../../util/godot/funcs.h"
#include "../../util/log.h"
#include "voxel_graph_editor.h"

#include <core/object/undo_redo.h>
#include <algorithm>

namespace zylann::voxel {

void VoxelGraphNodeInspectorWrapper::setup(
		Ref<VoxelGeneratorGraph> p_graph, uint32_t p_node_id, UndoRedo *ur, VoxelGraphEditor *ed) {
	_graph = p_graph;
	_node_id = p_node_id;
	_undo_redo = ur;
	_graph_editor = ed;
}

void VoxelGraphNodeInspectorWrapper::_get_property_list(List<PropertyInfo> *p_list) const {
	Ref<VoxelGeneratorGraph> graph = get_graph();
	ERR_FAIL_COND(graph.is_null());

	if (!graph->has_node(_node_id)) {
		// Maybe got erased by the user?
#ifdef DEBUG_ENABLED
		ZN_PRINT_VERBOSE("VoxelGeneratorGraph node was not found, from the graph inspector");
#endif
		return;
	}

	p_list->push_back(PropertyInfo(Variant::STRING_NAME, "name", PROPERTY_HINT_NONE, String(), PROPERTY_USAGE_EDITOR));

	const uint32_t node_type_id = graph->get_node_type_id(_node_id);
	const VoxelGraphNodeDB::NodeType &node_type = VoxelGraphNodeDB::get_singleton().get_type(node_type_id);

	// Params

	p_list->push_back(PropertyInfo(Variant::NIL, "Params", PROPERTY_HINT_NONE, String(), PROPERTY_USAGE_CATEGORY));

	for (size_t i = 0; i < node_type.params.size(); ++i) {
		const VoxelGraphNodeDB::Param &param = node_type.params[i];
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
		}
		pi.usage = PROPERTY_USAGE_EDITOR;
		p_list->push_back(pi);
	}

	// Inputs

	p_list->push_back(
			PropertyInfo(Variant::NIL, "Input Defaults", PROPERTY_HINT_NONE, String(), PROPERTY_USAGE_CATEGORY));

	for (size_t i = 0; i < node_type.inputs.size(); ++i) {
		const VoxelGraphNodeDB::Port &port = node_type.inputs[i];
		PropertyInfo pi;
		pi.name = port.name;
		pi.type = port.default_value.get_type();
		p_list->push_back(pi);
	}
}

// Automatically updates the list of inputs from variable names used in the expression.
// Contrary to VisualScript (for which this has to be done manually to the user), submitting the text field containing
// the expression's code also changes dynamic inputs of the node and reconnects existing connections, all as one
// UndoRedo action.
static void update_expression_inputs(
		VoxelGeneratorGraph &generator, uint32_t node_id, String code, UndoRedo &ur, VoxelGraphEditor &graph_editor) {
	//
	const CharString code_utf8 = code.utf8();
	std::vector<std::string_view> new_input_names;
	if (!VoxelGeneratorGraph::get_expression_variables(code_utf8.get_data(), new_input_names)) {
		// Error, the action will not include node input changes
		return;
	}
	std::vector<std::string> old_input_names;
	generator.get_expression_node_inputs(node_id, old_input_names);

	struct Connection {
		ProgramGraph::PortLocation src;
		uint32_t dst_port_index;
	};
	// Find what we'll disconnect
	std::vector<Connection> to_disconnect;
	for (uint32_t port_index = 0; port_index < old_input_names.size(); ++port_index) {
		ProgramGraph::PortLocation src;
		if (generator.try_get_connection_to({ node_id, port_index }, src)) {
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
			if (generator.try_get_connection_to({ node_id, port_index }, src)) {
				const uint32_t dst_port_index = new_input_name_it - new_input_names.begin();
				to_reconnect.push_back({ src, dst_port_index });
			}
		}
	}

	// Do

	for (size_t i = 0; i < to_disconnect.size(); ++i) {
		const Connection con = to_disconnect[i];
		ur.add_do_method(
				&generator, "remove_connection", con.src.node_id, con.src.port_index, node_id, con.dst_port_index);
	}

	ur.add_do_method(&generator, "set_expression_node_inputs", node_id, to_godot(new_input_names));

	for (size_t i = 0; i < to_reconnect.size(); ++i) {
		const Connection con = to_reconnect[i];
		ur.add_do_method(
				&generator, "add_connection", con.src.node_id, con.src.port_index, node_id, con.dst_port_index);
	}

	// Undo

	for (size_t i = 0; i < to_reconnect.size(); ++i) {
		const Connection con = to_reconnect[i];
		ur.add_undo_method(
				&generator, "remove_connection", con.src.node_id, con.src.port_index, node_id, con.dst_port_index);
	}

	ur.add_undo_method(&generator, "set_expression_node_inputs", node_id, to_godot(old_input_names));

	for (size_t i = 0; i < to_disconnect.size(); ++i) {
		const Connection con = to_disconnect[i];
		ur.add_undo_method(
				&generator, "add_connection", con.src.node_id, con.src.port_index, node_id, con.dst_port_index);
	}

	ur.add_do_method(&graph_editor, "update_node_layout", node_id);
	ur.add_undo_method(&graph_editor, "update_node_layout", node_id);
}

bool VoxelGraphNodeInspectorWrapper::_set(const StringName &p_name, const Variant &p_value) {
	Ref<VoxelGeneratorGraph> graph = get_graph();
	ERR_FAIL_COND_V(graph.is_null(), false);

	ERR_FAIL_COND_V(_undo_redo == nullptr, false);
	UndoRedo &ur = *_undo_redo;

	if (p_name == "name") {
		String previous_name = graph->get_node_name(_node_id);
		ur.create_action("Set VoxelGeneratorGraph node name");
		ur.add_do_method(graph.ptr(), "set_node_name", _node_id, p_value);
		ur.add_undo_method(graph.ptr(), "set_node_name", _node_id, previous_name);
		// ur->add_do_method(this, "notify_property_list_changed");
		// ur->add_undo_method(this, "notify_property_list_changed");
		ur.commit_action();
		return true;
	}

	const uint32_t node_type_id = graph->get_node_type_id(_node_id);

	const VoxelGraphNodeDB &db = VoxelGraphNodeDB::get_singleton();

	uint32_t index;
	if (db.try_get_param_index_from_name(node_type_id, p_name, index)) {
		Variant previous_value = graph->get_node_param(_node_id, index);
		ur.create_action("Set VoxelGeneratorGraph node parameter");
		ur.add_do_method(graph.ptr(), "set_node_param", _node_id, index, p_value);
		ur.add_undo_method(graph.ptr(), "set_node_param", _node_id, index, previous_value);
		if (node_type_id == VoxelGeneratorGraph::NODE_EXPRESSION) {
			update_expression_inputs(**graph, _node_id, p_value, ur, *_graph_editor);
			// TODO Default inputs cannot be set after adding variables!
			// It requires calling `notify_property_list_changed`, however that makes the LineEdit in the inspector to
			// reset its cursor position, making string parameter edition a nightmare. Only workaround is to deselect
			// and re-select the node...
		} else {
			ur.add_do_method(this, "notify_property_list_changed");
			ur.add_undo_method(this, "notify_property_list_changed");
		}
		ur.commit_action();

	} else if (db.try_get_input_index_from_name(node_type_id, p_name, index)) {
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
	Ref<VoxelGeneratorGraph> graph = get_graph();
	ERR_FAIL_COND_V(graph.is_null(), false);

	if (p_name == "name") {
		r_ret = graph->get_node_name(_node_id);
		return true;
	}

	const uint32_t node_type_id = graph->get_node_type_id(_node_id);

	const VoxelGraphNodeDB &db = VoxelGraphNodeDB::get_singleton();

	uint32_t index;
	if (db.try_get_param_index_from_name(node_type_id, p_name, index)) {
		r_ret = graph->get_node_param(_node_id, index);

	} else if (db.try_get_input_index_from_name(node_type_id, p_name, index)) {
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
