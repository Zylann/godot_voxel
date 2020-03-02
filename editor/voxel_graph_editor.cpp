#include "voxel_graph_editor.h"
#include "../generators/graph/voxel_generator_graph.h"
#include "../generators/graph/voxel_graph_node_db.h"
#include "editor/editor_scale.h"
#include <scene/gui/graph_edit.h>
#include <scene/gui/label.h>

class VoxelGraphEditorNode : public GraphNode {
	GDCLASS(VoxelGraphEditorNode, GraphNode)
public:
	uint32_t node_id = 0;
};

VoxelGraphEditor::VoxelGraphEditor() {
	_graph_edit = memnew(GraphEdit);
	_graph_edit->set_anchors_preset(Control::PRESET_WIDE);
	_graph_edit->set_right_disconnects(true);
	_graph_edit->connect("gui_input", this, "_on_graph_edit_gui_input");
	_graph_edit->connect("connection_request", this, "_on_graph_edit_connection_request");
	_graph_edit->connect("delete_nodes_request", this, "_on_graph_edit_delete_nodes_request");
	_graph_edit->connect("disconnection_request", this, "_on_graph_edit_disconnection_request");
	add_child(_graph_edit);

	_context_menu = memnew(PopupMenu);
	for (int i = 0; i < VoxelGraphNodeDB::get_singleton()->get_type_count(); ++i) {
		const VoxelGraphNodeDB::NodeType &node_type = VoxelGraphNodeDB::get_singleton()->get_type(i);
		_context_menu->add_item(node_type.name);
		_context_menu->set_item_metadata(i, i);
	}
	_context_menu->connect("index_pressed", this, "_on_context_menu_index_pressed");
	_context_menu->hide();
	add_child(_context_menu);
}

void VoxelGraphEditor::set_graph(Ref<VoxelGeneratorGraph> graph) {
	if (_graph == graph) {
		return;
	}

	//	if (_graph.is_valid()) {
	//	}

	_graph = graph;

	//	if (_graph.is_valid()) {
	//	}

	build_gui_from_graph();
}

void VoxelGraphEditor::clear() {
	_graph_edit->clear_connections();
	for (int i = 0; i < _graph_edit->get_child_count(); ++i) {
		Node *node = _graph_edit->get_child(i);
		GraphNode *node_view = Object::cast_to<GraphNode>(node);
		if (node_view != nullptr) {
			memdelete(node_view);
			--i;
		}
	}
}

inline String node_to_gui_name(uint32_t node_id) {
	return String("{0}").format(varray(node_id));
}

void VoxelGraphEditor::build_gui_from_graph() {
	// Rebuild the entire graph GUI

	clear();

	if (_graph.is_null()) {
		return;
	}

	const VoxelGeneratorGraph &graph = **_graph;

	// Nodes

	PoolIntArray node_ids = graph.get_node_ids();
	{
		PoolIntArray::Read node_ids_read = node_ids.read();
		for (int i = 0; i < node_ids.size(); ++i) {
			const uint32_t node_id = node_ids_read[i];
			build_gui_from_node(node_id);
		}
	}

	// Connections

	std::vector<ProgramGraph::Connection> connections;
	graph.get_connections(connections);

	for (size_t i = 0; i < connections.size(); ++i) {
		const ProgramGraph::Connection &con = connections[i];
		const String from_node_name = node_to_gui_name(con.src.node_id);
		const String to_node_name = node_to_gui_name(con.dst.node_id);
		const Error err = _graph_edit->connect_node(from_node_name, con.src.port_index, to_node_name, con.dst.port_index);
		ERR_FAIL_COND(err != OK);
	}
}

void VoxelGraphEditor::build_gui_from_node(uint32_t node_id) {
	// Build one GUI node

	CRASH_COND(_graph.is_null());
	const VoxelGeneratorGraph &graph = **_graph;
	const uint32_t node_type_id = graph.get_node_type_id(node_id);
	const VoxelGraphNodeDB::NodeType &node_type = VoxelGraphNodeDB::get_singleton()->get_type(node_type_id);

	const String node_name = node_to_gui_name(node_id);
	ERR_FAIL_COND(_graph_edit->has_node(node_name));

	VoxelGraphEditorNode *node_view = memnew(VoxelGraphEditorNode);
	node_view->set_offset(graph.get_node_gui_position(node_id));
	node_view->set_title(node_type.name);
	node_view->set_name(node_name);
	node_view->node_id = node_id;
	//node_view.resizable = true
	//node_view.rect_size = Vector2(200, 100)

	const int row_count = max(node_type.inputs.size(), node_type.outputs.size()) + node_type.params.size();
	const Color port_color(0.4, 0.4, 1.0);
	int param_index = 0;

	for (int i = 0; i < row_count; ++i) {
		const bool has_left = i < node_type.inputs.size();
		const bool has_right = i < node_type.outputs.size();

		HBoxContainer *property_control = memnew(HBoxContainer);
		property_control->set_custom_minimum_size(Vector2(0, 24 * EDSCALE));

		if (has_left) {
			Label *label = memnew(Label);
			label->set_text(node_type.inputs[i].name);
			property_control->add_child(label);

			// TODO Wire up edition
			SpinBox *spinbox = memnew(SpinBox);
			spinbox->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			property_control->add_child(spinbox);
		}

		if (has_right) {
			if (property_control->get_child_count() < 2) {
				Control *spacer = memnew(Control);
				spacer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
				property_control->add_child(spacer);
			}

			Label *label = memnew(Label);
			label->set_text(node_type.outputs[i].name);
			property_control->add_child(label);
		}

		if (!has_left && !has_right) {
			Label *label = memnew(Label);
			label->set_text(node_type.params[param_index].name);
			property_control->add_child(label);

			SpinBox *spinbox = memnew(SpinBox);
			spinbox->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			property_control->add_child(spinbox);

			++param_index;
		}

		node_view->add_child(property_control);
		node_view->set_slot(i, has_left, Variant::REAL, port_color, has_right, Variant::REAL, port_color);
	}

	_graph_edit->add_child(node_view);
}

void VoxelGraphEditor::_on_graph_edit_gui_input(Ref<InputEvent> event) {
	Ref<InputEventMouseButton> mb = event;

	if (mb.is_valid()) {
		if (mb->is_pressed()) {
			if (mb->get_button_index() == BUTTON_RIGHT) {
				_click_position = mb->get_position();
				_context_menu->set_position(get_global_mouse_position());
				_context_menu->popup();
			}
		}
	}
}

void VoxelGraphEditor::_on_graph_edit_connection_request(String from_node_name, int from_slot, String to_node_name, int to_slot) {
	VoxelGraphEditorNode *src_node_view = Object::cast_to<VoxelGraphEditorNode>(_graph_edit->get_node(from_node_name));
	VoxelGraphEditorNode *dst_node_view = Object::cast_to<VoxelGraphEditorNode>(_graph_edit->get_node(to_node_name));
	ERR_FAIL_COND(src_node_view == nullptr);
	ERR_FAIL_COND(dst_node_view == nullptr);
	//print("Connection attempt from ", from, ":", from_slot, " to ", to, ":", to_slot)
	if (_graph->can_connect(src_node_view->node_id, from_slot, dst_node_view->node_id, to_slot)) {
		_graph->add_connection(src_node_view->node_id, from_slot, dst_node_view->node_id, to_slot);
		_graph_edit->connect_node(from_node_name, from_slot, to_node_name, to_slot);
	}
}

void VoxelGraphEditor::_on_graph_edit_disconnection_request(String from_node_name, int from_slot, String to_node_name, int to_slot) {
	VoxelGraphEditorNode *src_node_view = Object::cast_to<VoxelGraphEditorNode>(_graph_edit->get_node(from_node_name));
	VoxelGraphEditorNode *dst_node_view = Object::cast_to<VoxelGraphEditorNode>(_graph_edit->get_node(to_node_name));
	ERR_FAIL_COND(src_node_view == nullptr);
	ERR_FAIL_COND(dst_node_view == nullptr);
	_graph->remove_connection(src_node_view->node_id, from_slot, dst_node_view->node_id, to_slot);
	_graph_edit->disconnect_node(from_node_name, from_slot, to_node_name, to_slot);
}

void remove_connections_from_and_to(GraphEdit *graph_edit, StringName node_name) {
	// Get copy of connection list
	List<GraphEdit::Connection> connections;
	graph_edit->get_connection_list(&connections);

	for (List<GraphEdit::Connection>::Element *E = connections.front(); E; E = E->next()) {
		const GraphEdit::Connection &con = E->get();
		if (con.from == node_name || con.to == node_name) {
			graph_edit->disconnect_node(con.from, con.from_port, con.to, con.to_port);
		}
	}
}

void VoxelGraphEditor::_on_graph_edit_delete_nodes_request() {

	std::vector<VoxelGraphEditorNode *> to_erase;

	for (int i = 0; i < _graph_edit->get_child_count(); ++i) {
		VoxelGraphEditorNode *node_view = Object::cast_to<VoxelGraphEditorNode>(_graph_edit->get_child(i));
		if (node_view != nullptr) {
			if (node_view->is_selected()) {
				to_erase.push_back(node_view);
			}
		}
	}

	for (size_t i = 0; i < to_erase.size(); ++i) {
		VoxelGraphEditorNode *node_view = to_erase[i];

		// Remove connections, because GraphNode doesn't do it...
		remove_connections_from_and_to(_graph_edit, node_view->get_name());

		_graph->remove_node(node_view->node_id);
		memdelete(node_view);
	}
}

Vector2 get_graph_offset_from_mouse(const GraphEdit *graph_edit, const Vector2 local_mouse_pos) {
	// TODO Ask for a method, or at least documentation about how it's done
	Vector2 offset = graph_edit->get_scroll_ofs() + local_mouse_pos;
	if (graph_edit->is_using_snap()) {
		const int snap = graph_edit->get_snap();
		offset = offset.snapped(Vector2(snap, snap));
	}
	offset /= EDSCALE;
	offset /= graph_edit->get_zoom();
	return offset;
}

void VoxelGraphEditor::_on_context_menu_index_pressed(int idx) {
	const Vector2 pos = get_graph_offset_from_mouse(_graph_edit, _click_position);
	const uint32_t node_type_id = _context_menu->get_item_metadata(idx);
	ERR_FAIL_INDEX(node_type_id, VoxelGeneratorGraph::NODE_TYPE_COUNT);
	const uint32_t node_id = _graph->create_node((VoxelGeneratorGraph::NodeTypeID)node_type_id, pos);
	build_gui_from_node(node_id);
}

void VoxelGraphEditor::_bind_methods() {

	ClassDB::bind_method(D_METHOD("_on_graph_edit_gui_input", "event"), &VoxelGraphEditor::_on_graph_edit_gui_input);
	ClassDB::bind_method(D_METHOD("_on_graph_edit_connection_request", "from_node_name", "from_slot", "to_node_name", "to_slot"),
			&VoxelGraphEditor::_on_graph_edit_connection_request);
	ClassDB::bind_method(D_METHOD("_on_graph_edit_disconnection_request", "from_node_name", "from_slot", "to_node_name", "to_slot"),
			&VoxelGraphEditor::_on_graph_edit_disconnection_request);
	ClassDB::bind_method(D_METHOD("_on_graph_edit_delete_nodes_request"), &VoxelGraphEditor::_on_graph_edit_delete_nodes_request);
	ClassDB::bind_method(D_METHOD("_on_context_menu_index_pressed", "idx"), &VoxelGraphEditor::_on_context_menu_index_pressed);
}
