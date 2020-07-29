#include "voxel_graph_editor.h"
#include "../../generators/graph/voxel_generator_graph.h"
#include "../../generators/graph/voxel_graph_node_db.h"
#include "../../util/macros.h"

#include <core/core_string_names.h>
#include <core/os/os.h>
#include <core/undo_redo.h>
#include <editor/editor_scale.h>
#include <scene/gui/graph_edit.h>
#include <scene/gui/label.h>

const char *VoxelGraphEditor::SIGNAL_NODE_SELECTED = "node_selected";
const char *VoxelGraphEditor::SIGNAL_NOTHING_SELECTED = "nothing_selected";

// Shows a 2D slice of the 3D set of values coming from an output port
class VoxelGraphEditorNodePreview : public VBoxContainer {
	GDCLASS(VoxelGraphEditorNodePreview, VBoxContainer)
public:
	static const int RESOLUTION = 64;

	VoxelGraphEditorNodePreview() {
		_image.instance();
		_image->create(RESOLUTION, RESOLUTION, false, Image::FORMAT_L8);
		_texture.instance();
		update_texture();
		_texture_rect = memnew(TextureRect);
		_texture_rect->set_stretch_mode(TextureRect::STRETCH_SCALE);
		_texture_rect->set_custom_minimum_size(Vector2(RESOLUTION, RESOLUTION));
		_texture_rect->set_texture(_texture);
		add_child(_texture_rect);
	}

	Ref<Image> get_image() const {
		return _image;
	}

	void update_texture() {
		_texture->create_from_image(_image, 0);
	}

private:
	TextureRect *_texture_rect = nullptr;
	Ref<ImageTexture> _texture;
	Ref<Image> _image;
};

// Graph node with a few custom data attached.
class VoxelGraphEditorNode : public GraphNode {
	GDCLASS(VoxelGraphEditorNode, GraphNode)
public:
	uint32_t node_id = 0;
	VoxelGraphEditorNodePreview *preview = nullptr;
};

VoxelGraphEditor::VoxelGraphEditor() {
	VBoxContainer *vbox_container = memnew(VBoxContainer);
	vbox_container->set_anchors_and_margins_preset(Control::PRESET_WIDE);

	{
		HBoxContainer *toolbar = memnew(HBoxContainer);

		Button *update_previews_button = memnew(Button);
		update_previews_button->set_text("Update Previews");
		update_previews_button->connect("pressed", this, "_on_update_previews_button_pressed");
		toolbar->add_child(update_previews_button);

		Button *profile_button = memnew(Button);
		profile_button->set_text("Profile");
		profile_button->connect("pressed", this, "_on_profile_button_pressed");
		toolbar->add_child(profile_button);

		_profile_label = memnew(Label);
		toolbar->add_child(_profile_label);

		vbox_container->add_child(toolbar);
	}

	_graph_edit = memnew(GraphEdit);
	_graph_edit->set_anchors_preset(Control::PRESET_WIDE);
	_graph_edit->set_right_disconnects(true);
	_graph_edit->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	_graph_edit->connect("gui_input", this, "_on_graph_edit_gui_input");
	_graph_edit->connect("connection_request", this, "_on_graph_edit_connection_request");
	_graph_edit->connect("delete_nodes_request", this, "_on_graph_edit_delete_nodes_request");
	_graph_edit->connect("disconnection_request", this, "_on_graph_edit_disconnection_request");
	_graph_edit->connect("node_selected", this, "_on_graph_edit_node_selected");
	_graph_edit->connect("node_unselected", this, "_on_graph_edit_node_unselected");
	vbox_container->add_child(_graph_edit);

	add_child(vbox_container);

	_context_menu = memnew(PopupMenu);
	FixedArray<PopupMenu *, VoxelGraphNodeDB::CATEGORY_COUNT> category_menus;
	for (unsigned int i = 0; i < category_menus.size(); ++i) {
		String name = VoxelGraphNodeDB::get_category_name(VoxelGraphNodeDB::Category(i));
		PopupMenu *menu = memnew(PopupMenu);
		menu->set_name(name);
		menu->connect("id_pressed", this, "_on_context_menu_id_pressed");
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
}

void VoxelGraphEditor::set_graph(Ref<VoxelGeneratorGraph> graph) {
	if (_graph == graph) {
		return;
	}

	if (_graph.is_valid()) {
		_graph->disconnect(CoreStringNames::get_singleton()->changed, this, "_on_graph_changed");
	}

	_graph = graph;

	if (_graph.is_valid()) {
		_graph->connect(CoreStringNames::get_singleton()->changed, this, "_on_graph_changed");
	}

	build_gui_from_graph();
}

void VoxelGraphEditor::set_undo_redo(UndoRedo *undo_redo) {
	_undo_redo = undo_redo;
}

void VoxelGraphEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_INTERNAL_PROCESS:
			_process(get_tree()->get_idle_process_time());
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
			create_node_gui(node_id);
		}
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
		const Error err = _graph_edit->connect_node(from_node_name, con.src.port_index, to_node_view->get_name(), con.dst.port_index);
		ERR_FAIL_COND(err != OK);
	}
}

void VoxelGraphEditor::create_node_gui(uint32_t node_id) {
	// Build one GUI node

	CRASH_COND(_graph.is_null());
	const VoxelGeneratorGraph &graph = **_graph;
	const uint32_t node_type_id = graph.get_node_type_id(node_id);
	const VoxelGraphNodeDB::NodeType &node_type = VoxelGraphNodeDB::get_singleton()->get_type(node_type_id);

	const String node_name = node_to_gui_name(node_id);
	ERR_FAIL_COND(_graph_edit->has_node(node_name));

	VoxelGraphEditorNode *node_view = memnew(VoxelGraphEditorNode);
	node_view->set_offset(graph.get_node_gui_position(node_id) * EDSCALE);
	node_view->set_title(node_type.name);
	node_view->set_name(node_name);
	node_view->node_id = node_id;
	node_view->connect("dragged", this, "_on_graph_node_dragged", varray(node_id));
	//node_view.resizable = true
	//node_view.rect_size = Vector2(200, 100)

	const unsigned int row_count = max(node_type.inputs.size(), node_type.outputs.size());
	const Color port_color(0.4, 0.4, 1.0);

	for (unsigned int i = 0; i < row_count; ++i) {
		const bool has_left = i < node_type.inputs.size();
		const bool has_right = i < node_type.outputs.size();

		HBoxContainer *property_control = memnew(HBoxContainer);
		property_control->set_custom_minimum_size(Vector2(0, 24 * EDSCALE));

		if (has_left) {
			Label *label = memnew(Label);
			label->set_text(node_type.inputs[i].name);
			property_control->add_child(label);
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

		node_view->add_child(property_control);
		node_view->set_slot(i, has_left, Variant::REAL, port_color, has_right, Variant::REAL, port_color);
	}

	if (node_type_id == VoxelGeneratorGraph::NODE_SDF_PREVIEW) {
		node_view->preview = memnew(VoxelGraphEditorNodePreview);
		node_view->add_child(node_view->preview);
	}

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
		_undo_redo->create_action(TTR("Connect Nodes"));

		_undo_redo->add_do_method(*_graph, "add_connection", src_node_view->node_id, from_slot, dst_node_view->node_id, to_slot);
		_undo_redo->add_do_method(_graph_edit, "connect_node", from_node_name, from_slot, to_node_name, to_slot);

		_undo_redo->add_undo_method(*_graph, "remove_connection", src_node_view->node_id, from_slot, dst_node_view->node_id, to_slot);
		_undo_redo->add_undo_method(_graph_edit, "disconnect_node", from_node_name, from_slot, to_node_name, to_slot);

		_undo_redo->commit_action();
	}
}

void VoxelGraphEditor::_on_graph_edit_disconnection_request(String from_node_name, int from_slot, String to_node_name, int to_slot) {
	VoxelGraphEditorNode *src_node_view = Object::cast_to<VoxelGraphEditorNode>(_graph_edit->get_node(from_node_name));
	VoxelGraphEditorNode *dst_node_view = Object::cast_to<VoxelGraphEditorNode>(_graph_edit->get_node(to_node_name));
	ERR_FAIL_COND(src_node_view == nullptr);
	ERR_FAIL_COND(dst_node_view == nullptr);

	_undo_redo->create_action(TTR("Disconnect Nodes"));

	_undo_redo->add_do_method(*_graph, "remove_connection", src_node_view->node_id, from_slot, dst_node_view->node_id, to_slot);
	_undo_redo->add_do_method(_graph_edit, "disconnect_node", from_node_name, from_slot, to_node_name, to_slot);

	_undo_redo->add_undo_method(*_graph, "add_connection", src_node_view->node_id, from_slot, dst_node_view->node_id, to_slot);
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
		const uint32_t node_id = node_view->node_id;
		const uint32_t node_type_id = _graph->get_node_type_id(node_id);

		_undo_redo->add_do_method(*_graph, "remove_node", node_id);
		_undo_redo->add_do_method(this, "remove_node_gui", node_view->get_name());

		_undo_redo->add_undo_method(*_graph, "create_node", node_type_id, _graph->get_node_gui_position(node_id), node_id);

		// Params undo
		const size_t param_count = VoxelGraphNodeDB::get_singleton()->get_type(node_type_id).params.size();
		for (size_t j = 0; j < param_count; ++j) {
			Variant param_value = _graph->get_node_param(node_id, j);
			_undo_redo->add_undo_method(*_graph, "set_node_param", node_id, j, param_value);
		}

		_undo_redo->add_undo_method(this, "create_node_gui", node_id);

		// Connections undo
		for (size_t j = 0; j < connections.size(); ++j) {
			const ProgramGraph::Connection &con = connections[j];
			if (con.src.node_id == node_id || con.dst.node_id == node_id) {
				_undo_redo->add_undo_method(*_graph, "add_connection", con.src.node_id, con.src.port_index, con.dst.node_id, con.dst.port_index);
				String src_node_name = node_to_gui_name(con.src.node_id);
				String dst_node_name = node_to_gui_name(con.dst.node_id);
				_undo_redo->add_undo_method(_graph_edit, "connect_node", src_node_name, con.src.port_index, dst_node_name, con.dst.port_index);
			}
		}
	}

	_undo_redo->commit_action();
}

void VoxelGraphEditor::_on_graph_node_dragged(Vector2 from, Vector2 to, int id) {
	_undo_redo->create_action(TTR("Move nodes"));
	_undo_redo->add_do_method(this, "set_node_position", id, to);
	_undo_redo->add_undo_method(this, "set_node_position", id, from);
	_undo_redo->commit_action();
	// I haven't the faintest idea how VisualScriptEditor magically makes this work neither using `create_action` nor `commit_action`.
}

void VoxelGraphEditor::set_node_position(int id, Vector2 offset) {
	String node_name = node_to_gui_name(id);
	GraphNode *node_view = Object::cast_to<GraphNode>(_graph_edit->get_node(node_name));
	if (node_view != nullptr) {
		node_view->set_offset(offset);
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
	emit_signal(SIGNAL_NODE_SELECTED, node->node_id);
}

void VoxelGraphEditor::_on_graph_edit_node_unselected(Node *p_node) {
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

void VoxelGraphEditor::update_previews() {
	if (_graph.is_null()) {
		return;
	}

	uint64_t time_before = OS::get_singleton()->get_ticks_usec();

	_graph->compile();

	// TODO Use a thread?
	PRINT_VERBOSE("Updating previews");

	struct PreviewInfo {
		VoxelGraphEditorNodePreview *control;
		uint16_t address;
		float min_value;
		float value_scale;
	};

	std::vector<PreviewInfo> previews;
	const VoxelGraphRuntime &runtime = _graph->get_runtime();

	for (int i = 0; i < _graph_edit->get_child_count(); ++i) {
		VoxelGraphEditorNode *node = Object::cast_to<VoxelGraphEditorNode>(_graph_edit->get_child(i));
		if (node == nullptr || node->preview == nullptr) {
			continue;
		}
		ProgramGraph::PortLocation dst;
		dst.node_id = node->node_id;
		dst.port_index = 0;
		ProgramGraph::PortLocation src;
		if (!_graph->try_get_connection_to(dst, src)) {
			// Not connected?
			continue;
		}
		PreviewInfo info;
		info.control = node->preview;
		info.address = runtime.get_output_port_address(src);
		info.min_value = _graph->get_node_param(dst.node_id, 0);
		const float max_value = _graph->get_node_param(dst.node_id, 1);
		info.value_scale = 1.f / (max_value - info.min_value);
		previews.push_back(info);
	}

	for (size_t i = 0; i < previews.size(); ++i) {
		previews[i].control->get_image()->lock();
	}

	for (int iy = 0; iy < VoxelGraphEditorNodePreview::RESOLUTION; ++iy) {
		for (int ix = 0; ix < VoxelGraphEditorNodePreview::RESOLUTION; ++ix) {
			{
				const int x = ix - VoxelGraphEditorNodePreview::RESOLUTION / 2;
				const int y = (VoxelGraphEditorNodePreview::RESOLUTION - iy) - VoxelGraphEditorNodePreview::RESOLUTION / 2;
				_graph->generate_single(Vector3i(x, y, 0));
			}

			for (size_t i = 0; i < previews.size(); ++i) {
				PreviewInfo &info = previews[i];
				const float v = runtime.get_memory_value(info.address);
				const float g = clamp((v - info.min_value) * info.value_scale, 0.f, 1.f);
				info.control->get_image()->set_pixel(ix, iy, Color(g, g, g));
			}
		}
	}

	for (size_t i = 0; i < previews.size(); ++i) {
		previews[i].control->get_image()->unlock();
		previews[i].control->update_texture();
	}

	uint64_t time_taken = OS::get_singleton()->get_ticks_usec() - time_before;
	PRINT_VERBOSE(String("Previews generated in {0} us").format(varray(time_taken)));
}

void VoxelGraphEditor::schedule_preview_update() {
	_time_before_preview_update = 0.5f;
}

void VoxelGraphEditor::_on_graph_changed() {
	schedule_preview_update();
}

void VoxelGraphEditor::_on_update_previews_button_pressed() {
	update_previews();
}

void VoxelGraphEditor::_on_profile_button_pressed() {
	if (_graph.is_null()) {
		return;
	}
	const float us = _graph->debug_measure_microseconds_per_voxel();
	_profile_label->set_text(String("{0} microseconds per voxel").format(varray(us)));
}

void VoxelGraphEditor::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_on_graph_edit_gui_input", "event"), &VoxelGraphEditor::_on_graph_edit_gui_input);
	ClassDB::bind_method(D_METHOD("_on_graph_edit_connection_request", "from_node_name", "from_slot", "to_node_name", "to_slot"),
			&VoxelGraphEditor::_on_graph_edit_connection_request);
	ClassDB::bind_method(D_METHOD("_on_graph_edit_disconnection_request", "from_node_name", "from_slot", "to_node_name", "to_slot"),
			&VoxelGraphEditor::_on_graph_edit_disconnection_request);
	ClassDB::bind_method(D_METHOD("_on_graph_edit_delete_nodes_request"), &VoxelGraphEditor::_on_graph_edit_delete_nodes_request);
	ClassDB::bind_method(D_METHOD("_on_graph_edit_node_selected"), &VoxelGraphEditor::_on_graph_edit_node_selected);
	ClassDB::bind_method(D_METHOD("_on_graph_edit_node_unselected"), &VoxelGraphEditor::_on_graph_edit_node_unselected);
	ClassDB::bind_method(D_METHOD("_on_graph_node_dragged", "from", "to", "id"), &VoxelGraphEditor::_on_graph_node_dragged);
	ClassDB::bind_method(D_METHOD("_on_context_menu_id_pressed", "id"), &VoxelGraphEditor::_on_context_menu_id_pressed);
	ClassDB::bind_method(D_METHOD("_on_graph_changed"), &VoxelGraphEditor::_on_graph_changed);
	ClassDB::bind_method(D_METHOD("_on_update_previews_button_pressed"), &VoxelGraphEditor::_on_update_previews_button_pressed);
	ClassDB::bind_method(D_METHOD("_on_profile_button_pressed"), &VoxelGraphEditor::_on_profile_button_pressed);

	ClassDB::bind_method(D_METHOD("_check_nothing_selected"), &VoxelGraphEditor::_check_nothing_selected);

	ClassDB::bind_method(D_METHOD("create_node_gui", "node_id"), &VoxelGraphEditor::create_node_gui);
	ClassDB::bind_method(D_METHOD("remove_node_gui", "node_name"), &VoxelGraphEditor::remove_node_gui);
	ClassDB::bind_method(D_METHOD("set_node_position", "node_id", "offset"), &VoxelGraphEditor::set_node_position);

	ADD_SIGNAL(MethodInfo(SIGNAL_NODE_SELECTED, PropertyInfo(Variant::INT, "node_id")));
	ADD_SIGNAL(MethodInfo(SIGNAL_NOTHING_SELECTED, PropertyInfo(Variant::INT, "nothing_selected")));
}
