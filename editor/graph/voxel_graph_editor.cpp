#include "voxel_graph_editor.h"
#include "../../generators/graph/voxel_generator_graph.h"
#include "../../generators/graph/voxel_graph_node_db.h"
#include "../../terrain/voxel_node.h"
#include "../../util/macros.h"
#include "voxel_graph_editor_node.h"
#include "voxel_graph_editor_node_preview.h"
#include "voxel_range_analysis_dialog.h"

#include <core/core_string_names.h>
#include <core/object/undo_redo.h>
#include <core/os/time.h>
#include <editor/editor_scale.h>
#include <scene/gui/graph_edit.h>
#include <scene/gui/label.h>
#include <scene/gui/option_button.h>

namespace zylann::voxel {

const char *VoxelGraphEditor::SIGNAL_NODE_SELECTED = "node_selected";
const char *VoxelGraphEditor::SIGNAL_NOTHING_SELECTED = "nothing_selected";
const char *VoxelGraphEditor::SIGNAL_NODES_DELETED = "nodes_deleted";

VoxelGraphEditor::VoxelGraphEditor() {
	VBoxContainer *vbox_container = memnew(VBoxContainer);
	vbox_container->set_anchors_and_offsets_preset(Control::PRESET_WIDE);

	{
		HBoxContainer *toolbar = memnew(HBoxContainer);

		Button *update_previews_button = memnew(Button);
		update_previews_button->set_text("Update Previews");
		update_previews_button->connect(
				"pressed", callable_mp(this, &VoxelGraphEditor::_on_update_previews_button_pressed));
		toolbar->add_child(update_previews_button);

		Button *profile_button = memnew(Button);
		profile_button->set_text("Profile");
		profile_button->connect("pressed", callable_mp(this, &VoxelGraphEditor::_on_profile_button_pressed));
		toolbar->add_child(profile_button);

		_profile_label = memnew(Label);
		toolbar->add_child(_profile_label);

		_compile_result_label = memnew(Label);
		_compile_result_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		_compile_result_label->set_clip_text(true);
		_compile_result_label->hide();
		toolbar->add_child(_compile_result_label);

		Button *range_analysis_button = memnew(Button);
		range_analysis_button->set_text("Analyze Range...");
		range_analysis_button->connect(
				"pressed", callable_mp(this, &VoxelGraphEditor::_on_analyze_range_button_pressed));
		toolbar->add_child(range_analysis_button);

		OptionButton *preview_axes_menu = memnew(OptionButton);
		preview_axes_menu->add_item("Preview XY", PREVIEW_XY);
		preview_axes_menu->add_item("Preview XZ", PREVIEW_XZ);
		preview_axes_menu->get_popup()->connect(
				"id_pressed", callable_mp(this, &VoxelGraphEditor::_on_preview_axes_menu_id_pressed));
		toolbar->add_child(preview_axes_menu);

		vbox_container->add_child(toolbar);
	}

	_graph_edit = memnew(GraphEdit);
	_graph_edit->set_anchors_preset(Control::PRESET_WIDE);
	_graph_edit->set_right_disconnects(true);
	_graph_edit->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	_graph_edit->connect("gui_input", callable_mp(this, &VoxelGraphEditor::_on_graph_edit_gui_input));
	_graph_edit->connect("connection_request", callable_mp(this, &VoxelGraphEditor::_on_graph_edit_connection_request));
	_graph_edit->connect(
			"delete_nodes_request", callable_mp(this, &VoxelGraphEditor::_on_graph_edit_delete_nodes_request));
	_graph_edit->connect(
			"disconnection_request", callable_mp(this, &VoxelGraphEditor::_on_graph_edit_disconnection_request));
	_graph_edit->connect("node_selected", callable_mp(this, &VoxelGraphEditor::_on_graph_edit_node_selected));
	_graph_edit->connect("node_deselected", callable_mp(this, &VoxelGraphEditor::_on_graph_edit_node_deselected));
	vbox_container->add_child(_graph_edit);

	add_child(vbox_container);

	_context_menu = memnew(PopupMenu);
	FixedArray<PopupMenu *, VoxelGraphNodeDB::CATEGORY_COUNT> category_menus;
	for (unsigned int i = 0; i < category_menus.size(); ++i) {
		String name = VoxelGraphNodeDB::get_category_name(VoxelGraphNodeDB::Category(i));
		PopupMenu *menu = memnew(PopupMenu);
		menu->set_name(name);
		menu->connect("id_pressed", callable_mp(this, &VoxelGraphEditor::_on_context_menu_id_pressed));
		_context_menu->add_child(menu);
		_context_menu->add_submenu_item(name, name, i);
		category_menus[i] = menu;
	}
	for (int i = 0; i < VoxelGraphNodeDB::get_singleton()->get_type_count(); ++i) {
		const VoxelGraphNodeDB::NodeType &node_type = VoxelGraphNodeDB::get_singleton()->get_type(i);
		PopupMenu *menu = category_menus[node_type.category];
		menu->add_item(node_type.name, i);
	}
	_context_menu->hide();
	add_child(_context_menu);

	_range_analysis_dialog = memnew(VoxelRangeAnalysisDialog);
	_range_analysis_dialog->connect(
			"analysis_toggled", callable_mp(this, &VoxelGraphEditor::_on_range_analysis_toggled));
	_range_analysis_dialog->connect(
			"area_changed", callable_mp(this, &VoxelGraphEditor::_on_range_analysis_area_changed));
	add_child(_range_analysis_dialog);
}

void VoxelGraphEditor::set_graph(Ref<VoxelGeneratorGraph> graph) {
	if (_graph == graph) {
		return;
	}

	if (_graph.is_valid()) {
		_graph->disconnect(
				CoreStringNames::get_singleton()->changed, callable_mp(this, &VoxelGraphEditor::_on_graph_changed));
		_graph->disconnect(VoxelGeneratorGraph::SIGNAL_NODE_NAME_CHANGED,
				callable_mp(this, &VoxelGraphEditor::_on_graph_node_name_changed));
	}

	_graph = graph;

	if (_graph.is_valid()) {
		// Load a default preset when creating new graphs.
		// TODO Downside is, an empty graph cannot be seen.
		// But Godot doesnt let us know if the resource has been created from the inspector or not
		if (_graph->get_nodes_count() == 0) {
			_graph->load_plane_preset();
		}
		_graph->connect(
				CoreStringNames::get_singleton()->changed, callable_mp(this, &VoxelGraphEditor::_on_graph_changed));
		_graph->connect(VoxelGeneratorGraph::SIGNAL_NODE_NAME_CHANGED,
				callable_mp(this, &VoxelGraphEditor::_on_graph_node_name_changed));
	}

	_debug_renderer.clear();

	build_gui_from_graph();
}

void VoxelGraphEditor::set_undo_redo(UndoRedo *undo_redo) {
	_undo_redo = undo_redo;
}

void VoxelGraphEditor::set_voxel_node(VoxelNode *node) {
	_voxel_node = node;
	if (_voxel_node == nullptr) {
		PRINT_VERBOSE("Reference node for VoxelGraph gizmos: null");
		_debug_renderer.set_world(nullptr);
	} else {
		PRINT_VERBOSE(String("Reference node for VoxelGraph gizmos: {0}").format(varray(node->get_path())));
		_debug_renderer.set_world(_voxel_node->get_world_3d().ptr());
	}
}

void VoxelGraphEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_INTERNAL_PROCESS:
			_process(get_tree()->get_process_time());
			break;

		case NOTIFICATION_VISIBILITY_CHANGED:
			set_process_internal(is_visible());
			break;
	}
}

void VoxelGraphEditor::_process(float delta) {
	if (_time_before_preview_update > 0.f) {
		_time_before_preview_update -= delta;
		if (_time_before_preview_update < 0.f) {
			update_previews();
		}
	}

	// When an input is left unconnected, it picks a default value. Input hints show this value.
	// It is otherwise shown in the inspector when the node is selected, but seeing them at a glance helps.
	// I decided to do by polling so all the code is here and there is no faffing around with signals.
	if (_graph.is_valid() && is_visible_in_tree()) {
		for (int child_node_index = 0; child_node_index < _graph_edit->get_child_count(); ++child_node_index) {
			Node *node = _graph_edit->get_child(child_node_index);
			VoxelGraphEditorNode *node_view = Object::cast_to<VoxelGraphEditorNode>(node);
			if (node_view != nullptr) {
				node_view->poll_default_inputs(**_graph);
			}
		}
	}
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

	PackedInt32Array node_ids = graph.get_node_ids();
	for (int i = 0; i < node_ids.size(); ++i) {
		const uint32_t node_id = node_ids[i];
		create_node_gui(node_id);
	}

	// Connections

	std::vector<ProgramGraph::Connection> connections;
	graph.get_connections(connections);

	for (size_t i = 0; i < connections.size(); ++i) {
		const ProgramGraph::Connection &con = connections[i];
		const String from_node_name = node_to_gui_name(con.src.node_id);
		const String to_node_name = node_to_gui_name(con.dst.node_id);
		VoxelGraphEditorNode *to_node_view = Object::cast_to<VoxelGraphEditorNode>(_graph_edit->get_node(to_node_name));
		ERR_FAIL_COND(to_node_view == nullptr);
		const Error err = _graph_edit->connect_node(
				from_node_name, con.src.port_index, to_node_view->get_name(), con.dst.port_index);
		ERR_FAIL_COND(err != OK);
	}
}

void VoxelGraphEditor::create_node_gui(uint32_t node_id) {
	// Build one GUI node

	CRASH_COND(_graph.is_null());

	const String ui_node_name = node_to_gui_name(node_id);
	ERR_FAIL_COND(_graph_edit->has_node(ui_node_name));

	VoxelGraphEditorNode *node_view = VoxelGraphEditorNode::create(**_graph, node_id);
	node_view->set_name(ui_node_name);
	node_view->connect("dragged", callable_mp(this, &VoxelGraphEditor::_on_graph_node_dragged), varray(node_id));

	_graph_edit->add_child(node_view);
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

static NodePath to_node_path(StringName sn) {
	Vector<StringName> path;
	path.push_back(sn);
	return NodePath(path, false);
}

void VoxelGraphEditor::remove_node_gui(StringName gui_node_name) {
	// Remove connections from the UI, because GraphNode doesn't do it...
	remove_connections_from_and_to(_graph_edit, gui_node_name);
	Node *node_view = _graph_edit->get_node(to_node_path(gui_node_name));
	ERR_FAIL_COND(Object::cast_to<GraphNode>(node_view) == nullptr);
	memdelete(node_view);
}

// There is no API for this (and no internal function either), so like the implementation, I copy/pasted it
// static const GraphNode *get_graph_node_under_mouse(const GraphEdit *graph_edit) {
// 	for (int i = graph_edit->get_child_count() - 1; i >= 0; i--) {
// 		const GraphNode *gn = Object::cast_to<GraphNode>(graph_edit->get_child(i));
// 		if (gn != nullptr) {
// 			Rect2 r = gn->get_rect();
// 			r.size *= graph_edit->get_zoom();
// 			if (r.has_point(graph_edit->get_local_mouse_position())) {
// 				return gn;
// 			}
// 		}
// 	}
// 	return nullptr;
// }

static bool is_nothing_selected(GraphEdit *graph_edit) {
	for (int i = 0; i < graph_edit->get_child_count(); ++i) {
		GraphNode *node = Object::cast_to<GraphNode>(graph_edit->get_child(i));
		if (node != nullptr && node->is_selected()) {
			return false;
		}
	}
	return true;
}

void VoxelGraphEditor::_on_graph_edit_gui_input(Ref<InputEvent> event) {
	Ref<InputEventMouseButton> mb = event;

	if (mb.is_valid()) {
		if (mb->is_pressed()) {
			if (mb->get_button_index() == MouseButton::RIGHT) {
				_click_position = mb->get_position();
				_context_menu->set_position(get_global_mouse_position());
				_context_menu->popup();
			}
		}
	}
}

void VoxelGraphEditor::_on_graph_edit_connection_request(
		String from_node_name, int from_slot, String to_node_name, int to_slot) {
	//
	VoxelGraphEditorNode *src_node_view = Object::cast_to<VoxelGraphEditorNode>(_graph_edit->get_node(from_node_name));
	VoxelGraphEditorNode *dst_node_view = Object::cast_to<VoxelGraphEditorNode>(_graph_edit->get_node(to_node_name));
	ERR_FAIL_COND(src_node_view == nullptr);
	ERR_FAIL_COND(dst_node_view == nullptr);

	const uint32_t src_node_id = src_node_view->get_generator_node_id();
	const uint32_t dst_node_id = dst_node_view->get_generator_node_id();

	//print("Connection attempt from ", from, ":", from_slot, " to ", to, ":", to_slot)

	if (!_graph->is_valid_connection(src_node_id, from_slot, dst_node_id, to_slot)) {
		PRINT_VERBOSE("Connection is invalid");
		return;
	}

	_undo_redo->create_action(TTR("Connect Nodes"));

	ProgramGraph::PortLocation prev_src_port;
	String prev_src_node_name;
	const bool replacing =
			_graph->try_get_connection_to(ProgramGraph::PortLocation{ dst_node_id, uint32_t(to_slot) }, prev_src_port);

	if (replacing) {
		// Remove existing connection so we can replace with the new one
		prev_src_node_name = node_to_gui_name(prev_src_port.node_id);
		_undo_redo->add_do_method(
				*_graph, "remove_connection", prev_src_port.node_id, prev_src_port.port_index, dst_node_id, to_slot);
		_undo_redo->add_do_method(
				_graph_edit, "disconnect_node", prev_src_node_name, prev_src_port.port_index, to_node_name, to_slot);
	}

	_undo_redo->add_do_method(*_graph, "add_connection", src_node_id, from_slot, dst_node_id, to_slot);
	_undo_redo->add_do_method(_graph_edit, "connect_node", from_node_name, from_slot, to_node_name, to_slot);

	_undo_redo->add_undo_method(*_graph, "remove_connection", src_node_id, from_slot, dst_node_id, to_slot);
	_undo_redo->add_undo_method(_graph_edit, "disconnect_node", from_node_name, from_slot, to_node_name, to_slot);

	if (replacing) {
		// After undoing the connection we added, put back the connection we replaced
		_undo_redo->add_undo_method(
				*_graph, "add_connection", prev_src_port.node_id, prev_src_port.port_index, dst_node_id, to_slot);
		_undo_redo->add_undo_method(
				_graph_edit, "connect_node", prev_src_node_name, prev_src_port.port_index, to_node_name, to_slot);
	}

	_undo_redo->commit_action();
}

void VoxelGraphEditor::_on_graph_edit_disconnection_request(
		String from_node_name, int from_slot, String to_node_name, int to_slot) {
	VoxelGraphEditorNode *src_node_view = Object::cast_to<VoxelGraphEditorNode>(_graph_edit->get_node(from_node_name));
	VoxelGraphEditorNode *dst_node_view = Object::cast_to<VoxelGraphEditorNode>(_graph_edit->get_node(to_node_name));
	ERR_FAIL_COND(src_node_view == nullptr);
	ERR_FAIL_COND(dst_node_view == nullptr);

	const uint32_t src_node_id = src_node_view->get_generator_node_id();
	const uint32_t dst_node_id = dst_node_view->get_generator_node_id();

	_undo_redo->create_action(TTR("Disconnect Nodes"));

	_undo_redo->add_do_method(*_graph, "remove_connection", src_node_id, from_slot, dst_node_id, to_slot);
	_undo_redo->add_do_method(_graph_edit, "disconnect_node", from_node_name, from_slot, to_node_name, to_slot);

	_undo_redo->add_undo_method(*_graph, "add_connection", src_node_id, from_slot, dst_node_id, to_slot);
	_undo_redo->add_undo_method(_graph_edit, "connect_node", from_node_name, from_slot, to_node_name, to_slot);

	_undo_redo->commit_action();
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

	_undo_redo->create_action(TTR("Delete Nodes"));

	std::vector<ProgramGraph::Connection> connections;
	_graph->get_connections(connections);

	for (size_t i = 0; i < to_erase.size(); ++i) {
		const VoxelGraphEditorNode *node_view = to_erase[i];
		const uint32_t node_id = node_view->get_generator_node_id();
		const uint32_t node_type_id = _graph->get_node_type_id(node_id);

		_undo_redo->add_do_method(*_graph, "remove_node", node_id);
		_undo_redo->add_do_method(this, "remove_node_gui", node_view->get_name());

		_undo_redo->add_undo_method(
				*_graph, "create_node", node_type_id, _graph->get_node_gui_position(node_id), node_id);

		// Params undo
		const size_t param_count = VoxelGraphNodeDB::get_singleton()->get_type(node_type_id).params.size();
		for (size_t j = 0; j < param_count; ++j) {
			Variant param_value = _graph->get_node_param(node_id, j);
			_undo_redo->add_undo_method(*_graph, "set_node_param", node_id, SIZE_T_TO_VARIANT(j), param_value);
		}

		_undo_redo->add_undo_method(this, "create_node_gui", node_id);

		// Connections undo
		for (size_t j = 0; j < connections.size(); ++j) {
			const ProgramGraph::Connection &con = connections[j];

			if (con.src.node_id == node_id || con.dst.node_id == node_id) {
				_undo_redo->add_undo_method(*_graph, "add_connection", con.src.node_id, con.src.port_index,
						con.dst.node_id, con.dst.port_index);

				const String src_node_name = node_to_gui_name(con.src.node_id);
				const String dst_node_name = node_to_gui_name(con.dst.node_id);
				_undo_redo->add_undo_method(_graph_edit, "connect_node", src_node_name, con.src.port_index,
						dst_node_name, con.dst.port_index);
			}
		}
	}

	_undo_redo->commit_action();

	emit_signal(SIGNAL_NODES_DELETED);
}

void VoxelGraphEditor::_on_graph_node_dragged(Vector2 from, Vector2 to, int id) {
	_undo_redo->create_action(TTR("Move nodes"));
	_undo_redo->add_do_method(this, "set_node_position", id, to);
	_undo_redo->add_undo_method(this, "set_node_position", id, from);
	_undo_redo->commit_action();
	// I haven't the faintest idea how VisualScriptEditor magically makes this work,
	// neither using `create_action` nor `commit_action`.
}

void VoxelGraphEditor::set_node_position(int id, Vector2 offset) {
	String node_name = node_to_gui_name(id);
	GraphNode *node_view = Object::cast_to<GraphNode>(_graph_edit->get_node(node_name));
	if (node_view != nullptr) {
		node_view->set_position_offset(offset);
	}
	_graph->set_node_gui_position(id, offset / EDSCALE);
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

void VoxelGraphEditor::_on_context_menu_id_pressed(int id) {
	const Vector2 pos = get_graph_offset_from_mouse(_graph_edit, _click_position);
	const uint32_t node_type_id = id;
	const uint32_t node_id = _graph->generate_node_id();
	const StringName node_name = node_to_gui_name(node_id);

	_undo_redo->create_action(TTR("Create Node"));
	_undo_redo->add_do_method(*_graph, "create_node", node_type_id, pos, node_id);
	_undo_redo->add_do_method(this, "create_node_gui", node_id);
	_undo_redo->add_undo_method(*_graph, "remove_node", node_id);
	_undo_redo->add_undo_method(this, "remove_node_gui", node_name);
	_undo_redo->commit_action();
}

void VoxelGraphEditor::_on_graph_edit_node_selected(Node *p_node) {
	VoxelGraphEditorNode *node = Object::cast_to<VoxelGraphEditorNode>(p_node);
	emit_signal(SIGNAL_NODE_SELECTED, node->get_generator_node_id());
}

void VoxelGraphEditor::_on_graph_edit_node_deselected(Node *p_node) {
	// Just checking if nothing is selected _now_ is unreliable, because the user could have just selected another
	// node, and I don't know when `GraphEdit` will update the `selected` flags in the current call stack.
	// GraphEdit doesn't have an API giving us enough context to guess that, so have to rely on dirty workaround.
	if (!_nothing_selected_check_scheduled) {
		_nothing_selected_check_scheduled = true;
		call_deferred("_check_nothing_selected");
	}
}

void VoxelGraphEditor::_check_nothing_selected() {
	_nothing_selected_check_scheduled = false;
	if (is_nothing_selected(_graph_edit)) {
		emit_signal(SIGNAL_NOTHING_SELECTED);
	}
}

void reset_modulates(GraphEdit &graph_edit) {
	for (int child_index = 0; child_index < graph_edit.get_child_count(); ++child_index) {
		VoxelGraphEditorNode *node_view = Object::cast_to<VoxelGraphEditorNode>(graph_edit.get_child(child_index));
		if (node_view == nullptr) {
			continue;
		}
		node_view->set_modulate(Color(1, 1, 1));
	}
}

void VoxelGraphEditor::update_previews() {
	if (_graph.is_null()) {
		return;
	}

	clear_range_analysis_tooltips();
	reset_modulates(*_graph_edit);

	uint64_t time_before = Time::get_singleton()->get_ticks_usec();

	const VoxelGraphRuntime::CompilationResult result = _graph->compile();
	if (!result.success) {
		ERR_PRINT(String("Voxel graph compilation failed: {0}").format(varray(result.message)));

		_compile_result_label->set_text(result.message);
		_compile_result_label->set_tooltip(result.message);
		_compile_result_label->set_modulate(Color(1, 0.3, 0.1));
		_compile_result_label->show();

		if (result.node_id >= 0) {
			String node_view_path = node_to_gui_name(result.node_id);
			VoxelGraphEditorNode *node_view =
					Object::cast_to<VoxelGraphEditorNode>(_graph_edit->get_node(node_view_path));
			node_view->set_modulate(Color(1, 0.3, 0.1));
		}
		return;

	} else {
		_compile_result_label->hide();
	}

	if (!_graph->is_good()) {
		return;
	}

	// We assume no other thread will try to modify the graph and compile something not good

	update_slice_previews();

	if (_range_analysis_dialog->is_analysis_enabled()) {
		update_range_analysis_previews();
	}

	uint64_t time_taken = Time::get_singleton()->get_ticks_usec() - time_before;
	PRINT_VERBOSE(String("Previews generated in {0} us").format(varray(time_taken)));
}

void VoxelGraphEditor::update_range_analysis_previews() {
	PRINT_VERBOSE("Updating range analysis previews");
	ERR_FAIL_COND(_graph.is_null());
	ERR_FAIL_COND(!_graph->is_good());

	const AABB aabb = _range_analysis_dialog->get_aabb();
	_graph->debug_analyze_range(
			Vector3iUtil::from_floored(aabb.position), Vector3iUtil::from_floored(aabb.position + aabb.size), true);

	const VoxelGraphRuntime::State &state = _graph->get_last_state_from_current_thread();

	const Color greyed_out_color(1, 1, 1, 0.5);

	for (int child_index = 0; child_index < _graph_edit->get_child_count(); ++child_index) {
		VoxelGraphEditorNode *node_view = Object::cast_to<VoxelGraphEditorNode>(_graph_edit->get_child(child_index));
		if (node_view == nullptr) {
			continue;
		}

		if (!node_view->has_outputs()) {
			continue;
		}

		// Assume the node won't run for now
		// TODO Would be nice if GraphEdit's minimap would take such coloring into account...
		node_view->set_modulate(greyed_out_color);

		node_view->update_range_analysis_tooltips(**_graph, state);
	}

	// Highlight only nodes that will actually run
	Span<const int> execution_map = VoxelGeneratorGraph::get_last_execution_map_debug_from_current_thread();
	for (unsigned int i = 0; i < execution_map.size(); ++i) {
		String node_view_path = node_to_gui_name(execution_map[i]);
		VoxelGraphEditorNode *node_view = Object::cast_to<VoxelGraphEditorNode>(_graph_edit->get_node(node_view_path));
		node_view->set_modulate(Color(1, 1, 1));
	}
}

void VoxelGraphEditor::update_range_analysis_gizmo() {
	if (!_range_analysis_dialog->is_analysis_enabled()) {
		_debug_renderer.clear();
		return;
	}

	if (_voxel_node == nullptr) {
		return;
	}

	const Transform3D parent_transform = _voxel_node->get_global_transform();
	const AABB aabb = _range_analysis_dialog->get_aabb();
	_debug_renderer.begin();
	_debug_renderer.draw_box(parent_transform * Transform3D(Basis().scaled(aabb.size), aabb.position),
			DebugColors::ID_VOXEL_GRAPH_DEBUG_BOUNDS);
	_debug_renderer.end();
}

void VoxelGraphEditor::update_slice_previews() {
	// TODO Use a thread?
	PRINT_VERBOSE("Updating slice previews");
	ERR_FAIL_COND(!_graph->is_good());

	struct PreviewInfo {
		VoxelGraphEditorNodePreview *control;
		uint32_t address;
		float min_value;
		float value_scale;
	};

	std::vector<PreviewInfo> previews;

	// Gather preview nodes
	for (int i = 0; i < _graph_edit->get_child_count(); ++i) {
		VoxelGraphEditorNode *node = Object::cast_to<VoxelGraphEditorNode>(_graph_edit->get_child(i));
		if (node == nullptr || node->get_preview() == nullptr) {
			continue;
		}
		ProgramGraph::PortLocation dst;
		dst.node_id = node->get_generator_node_id();
		dst.port_index = 0;
		ProgramGraph::PortLocation src;
		if (!_graph->try_get_connection_to(dst, src)) {
			// Not connected?
			continue;
		}
		PreviewInfo info;
		info.control = node->get_preview();
		if (!_graph->try_get_output_port_address(src, info.address)) {
			// Not part of the compiled result
			continue;
		}
		info.min_value = _graph->get_node_param(dst.node_id, 0);
		const float max_value = _graph->get_node_param(dst.node_id, 1);
		info.value_scale = 1.f / (max_value - info.min_value);
		previews.push_back(info);
	}

	// Generate data
	{
		const int preview_size_x = VoxelGraphEditorNodePreview::RESOLUTION;
		const int preview_size_y = VoxelGraphEditorNodePreview::RESOLUTION;
		const int buffer_size = preview_size_x * preview_size_y;
		std::vector<float> x_vec;
		std::vector<float> y_vec;
		std::vector<float> z_vec;
		x_vec.resize(buffer_size);
		y_vec.resize(buffer_size);
		z_vec.resize(buffer_size);

		const Vector3f min_pos(-preview_size_x / 2, -preview_size_y / 2, 0);
		const Vector3f max_pos = min_pos + Vector3f(preview_size_x, preview_size_x, 0);

		int i = 0;
		for (int iy = 0; iy < preview_size_x; ++iy) {
			const float y = Math::lerp(min_pos.y, max_pos.y, static_cast<float>(iy) / preview_size_y);
			for (int ix = 0; ix < preview_size_y; ++ix) {
				const float x = Math::lerp(min_pos.x, max_pos.x, static_cast<float>(ix) / preview_size_x);
				x_vec[i] = x;
				y_vec[i] = y;
				z_vec[i] = min_pos.z;
				++i;
			}
		}

		Span<float> x_coords = to_span(x_vec);
		Span<float> y_coords;
		Span<float> z_coords;
		if (_preview_axes == PREVIEW_XY) {
			y_coords = to_span(y_vec);
			z_coords = to_span(z_vec);
		} else {
			y_coords = to_span(z_vec);
			z_coords = to_span(y_vec);
		}

		_graph->generate_set(x_coords, y_coords, z_coords);
	}

	const VoxelGraphRuntime::State &last_state = VoxelGeneratorGraph::get_last_state_from_current_thread();

	// Update previews
	for (size_t preview_index = 0; preview_index < previews.size(); ++preview_index) {
		PreviewInfo &info = previews[preview_index];

		const VoxelGraphRuntime::Buffer &buffer = last_state.get_buffer(info.address);

		Image &im = **info.control->get_image();
		ERR_FAIL_COND(im.get_width() * im.get_height() != static_cast<int>(buffer.size));

		unsigned int i = 0;
		for (int y = 0; y < im.get_height(); ++y) {
			for (int x = 0; x < im.get_width(); ++x) {
				const float v = buffer.data[i];
				const float g = math::clamp((v - info.min_value) * info.value_scale, 0.f, 1.f);
				im.set_pixel(x, im.get_height() - y - 1, Color(g, g, g));
				++i;
			}
		}

		info.control->update_texture();
	}
}

void VoxelGraphEditor::clear_range_analysis_tooltips() {
	for (int child_index = 0; child_index < _graph_edit->get_child_count(); ++child_index) {
		VoxelGraphEditorNode *node_view = Object::cast_to<VoxelGraphEditorNode>(_graph_edit->get_child(child_index));
		if (node_view == nullptr) {
			continue;
		}
		node_view->clear_range_analysis_tooltips();
	}
}

void VoxelGraphEditor::schedule_preview_update() {
	_time_before_preview_update = 0.5f;
}

void VoxelGraphEditor::_on_graph_changed() {
	schedule_preview_update();
}

void VoxelGraphEditor::_on_graph_node_name_changed(int node_id) {
	ERR_FAIL_COND(_graph.is_null());
	StringName node_name = _graph->get_node_name(node_id);

	const uint32_t node_type_id = _graph->get_node_type_id(node_id);
	String node_type_name = VoxelGraphNodeDB::get_singleton()->get_type(node_type_id).name;

	const String ui_node_name = node_to_gui_name(node_id);
	VoxelGraphEditorNode *node_view = Object::cast_to<VoxelGraphEditorNode>(_graph_edit->get_node(ui_node_name));
	ERR_FAIL_COND(node_view == nullptr);

	node_view->update_title(node_name, node_type_name);
}

void VoxelGraphEditor::_on_update_previews_button_pressed() {
	update_previews();
}

void VoxelGraphEditor::_on_profile_button_pressed() {
	if (_graph.is_null()) {
		return;
	}
	const float us = _graph->debug_measure_microseconds_per_voxel(false);
	_profile_label->set_text(String("{0} microseconds per voxel").format(varray(us)));
}

void VoxelGraphEditor::_on_analyze_range_button_pressed() {
	_range_analysis_dialog->popup_centered();
}

void VoxelGraphEditor::_on_range_analysis_toggled(bool enabled) {
	schedule_preview_update();
	update_range_analysis_gizmo();
}

void VoxelGraphEditor::_on_range_analysis_area_changed() {
	schedule_preview_update();
	update_range_analysis_gizmo();
}

void VoxelGraphEditor::_on_preview_axes_menu_id_pressed(int id) {
	ERR_FAIL_COND(id < 0 || id >= PREVIEW_AXES_OPTIONS_COUNT);
	_preview_axes = PreviewAxes(id);
	schedule_preview_update();
}

void VoxelGraphEditor::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_check_nothing_selected"), &VoxelGraphEditor::_check_nothing_selected);

	ClassDB::bind_method(D_METHOD("create_node_gui", "node_id"), &VoxelGraphEditor::create_node_gui);
	ClassDB::bind_method(D_METHOD("remove_node_gui", "node_name"), &VoxelGraphEditor::remove_node_gui);
	ClassDB::bind_method(D_METHOD("set_node_position", "node_id", "offset"), &VoxelGraphEditor::set_node_position);

	ADD_SIGNAL(MethodInfo(SIGNAL_NODE_SELECTED, PropertyInfo(Variant::INT, "node_id")));
	ADD_SIGNAL(MethodInfo(SIGNAL_NOTHING_SELECTED, PropertyInfo(Variant::INT, "nothing_selected")));
	ADD_SIGNAL(MethodInfo(SIGNAL_NODES_DELETED, PropertyInfo(Variant::INT, "nodes_deleted")));
}

} // namespace zylann::voxel
