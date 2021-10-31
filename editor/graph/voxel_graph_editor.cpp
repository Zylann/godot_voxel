#include "voxel_graph_editor.h"
#include "../../generators/graph/voxel_generator_graph.h"
#include "../../generators/graph/voxel_graph_node_db.h"
#include "../../terrain/voxel_node.h"
#include "../../util/macros.h"

#include <core/core_string_names.h>
#include <core/os/os.h>
#include <core/undo_redo.h>
#include <editor/editor_scale.h>
#include <scene/gui/check_box.h>
#include <scene/gui/dialogs.h>
#include <scene/gui/graph_edit.h>
#include <scene/gui/grid_container.h>
#include <scene/gui/label.h>

const char *VoxelGraphEditor::SIGNAL_NODE_SELECTED = "node_selected";
const char *VoxelGraphEditor::SIGNAL_NOTHING_SELECTED = "nothing_selected";

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Shows a 2D slice of the 3D set of values coming from an output port
class VoxelGraphEditorNodePreview : public VBoxContainer {
	GDCLASS(VoxelGraphEditorNodePreview, VBoxContainer)
public:
	static const int RESOLUTION = 128;

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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Graph node with a few custom data attached.
class VoxelGraphEditorNode : public GraphNode {
	GDCLASS(VoxelGraphEditorNode, GraphNode)
public:
	uint32_t node_id = 0;
	VoxelGraphEditorNodePreview *preview = nullptr;
	Vector<Control *> output_labels;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class VoxelRangeAnalysisDialog : public AcceptDialog {
	GDCLASS(VoxelRangeAnalysisDialog, AcceptDialog)
public:
	VoxelRangeAnalysisDialog() {
		set_title(TTR("Debug Range Analysis"));
		set_custom_minimum_size(EDSCALE * Vector2(300, 280));

		VBoxContainer *vb = memnew(VBoxContainer);
		//vb->set_anchors_preset(Control::PRESET_TOP_WIDE);

		enabled_checkbox = memnew(CheckBox);
		enabled_checkbox->set_text(TTR("Enabled"));
		enabled_checkbox->connect("toggled", this, "_on_enabled_checkbox_toggled");
		vb->add_child(enabled_checkbox);

		Label *tip = memnew(Label);
		// TODO Had to use `\n` and disable autowrap, otherwise the popup height becomes crazy high
		// See https://github.com/godotengine/godot/issues/47005
		tip->set_text(TTR("When enabled, hover node output labels to\ninspect their "
						  "estimated range within the\nconfigured area.\n"
						  "Nodes that may be optimized out locally will be greyed out."));
		//tip->set_autowrap(true);
		tip->set_modulate(Color(1.f, 1.f, 1.f, 0.8f));
		vb->add_child(tip);

		GridContainer *gc = memnew(GridContainer);
		gc->set_anchors_preset(Control::PRESET_TOP_WIDE);
		gc->set_columns(2);

		add_row(TTR("Position X"), pos_x_spinbox, gc, 0);
		add_row(TTR("Position Y"), pos_y_spinbox, gc, 0);
		add_row(TTR("Position Z"), pos_z_spinbox, gc, 0);
		add_row(TTR("Size X"), size_x_spinbox, gc, 100);
		add_row(TTR("Size Y"), size_y_spinbox, gc, 100);
		add_row(TTR("Size Z"), size_z_spinbox, gc, 100);

		vb->add_child(gc);

		add_child(vb);
	}

	AABB get_aabb() const {
		const Vector3 center(pos_x_spinbox->get_value(), pos_y_spinbox->get_value(), pos_z_spinbox->get_value());
		const Vector3 size(size_x_spinbox->get_value(), size_y_spinbox->get_value(), size_z_spinbox->get_value());
		return AABB(center - 0.5f * size, size);
	}

	bool is_analysis_enabled() const {
		return enabled_checkbox->is_pressed();
	}

private:
	void _on_enabled_checkbox_toggled(bool enabled) {
		emit_signal("analysis_toggled", enabled);
	}

	void _on_area_spinbox_value_changed(float value) {
		emit_signal("area_changed");
	}

	void add_row(String text, SpinBox *&sb, GridContainer *parent, float defval) {
		sb = memnew(SpinBox);
		sb->set_min(-99999.f);
		sb->set_max(99999.f);
		sb->set_value(defval);
		sb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		Label *label = memnew(Label);
		label->set_text(text);
		parent->add_child(label);
		parent->add_child(sb);
		sb->connect("value_changed", this, "_on_area_spinbox_value_changed");
	}

	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("_on_enabled_checkbox_toggled", "enabled"),
				&VoxelRangeAnalysisDialog::_on_enabled_checkbox_toggled);
		ClassDB::bind_method(D_METHOD("_on_area_spinbox_value_changed", "value"),
				&VoxelRangeAnalysisDialog::_on_area_spinbox_value_changed);

		ADD_SIGNAL(MethodInfo("analysis_toggled", PropertyInfo(Variant::BOOL, "enabled")));
		ADD_SIGNAL(MethodInfo("area_changed"));
	}

	CheckBox *enabled_checkbox;
	SpinBox *pos_x_spinbox;
	SpinBox *pos_y_spinbox;
	SpinBox *pos_z_spinbox;
	SpinBox *size_x_spinbox;
	SpinBox *size_y_spinbox;
	SpinBox *size_z_spinbox;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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

		_compile_result_label = memnew(Label);
		_compile_result_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		_compile_result_label->set_clip_text(true);
		_compile_result_label->hide();
		toolbar->add_child(_compile_result_label);

		Button *range_analysis_button = memnew(Button);
		range_analysis_button->set_text("Analyze Range...");
		range_analysis_button->connect("pressed", this, "_on_analyze_range_button_pressed");
		toolbar->add_child(range_analysis_button);

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

	_range_analysis_dialog = memnew(VoxelRangeAnalysisDialog);
	_range_analysis_dialog->connect("analysis_toggled", this, "_on_range_analysis_toggled");
	_range_analysis_dialog->connect("area_changed", this, "_on_range_analysis_area_changed");
	add_child(_range_analysis_dialog);
}

void VoxelGraphEditor::set_graph(Ref<VoxelGeneratorGraph> graph) {
	if (_graph == graph) {
		return;
	}

	if (_graph.is_valid()) {
		_graph->disconnect(CoreStringNames::get_singleton()->changed, this, "_on_graph_changed");
		_graph->disconnect(VoxelGeneratorGraph::SIGNAL_NODE_NAME_CHANGED, this, "_on_graph_node_name_changed");
	}

	_graph = graph;

	if (_graph.is_valid()) {
		// Load a default preset when creating new graphs.
		// TODO Downside is, an empty graph cannot be seen.
		// But Godot doesnt let us know if the resource has been created from the inspector or not
		if (_graph->get_nodes_count() == 0) {
			_graph->load_plane_preset();
		}
		_graph->connect(CoreStringNames::get_singleton()->changed, this, "_on_graph_changed");
		_graph->connect(VoxelGeneratorGraph::SIGNAL_NODE_NAME_CHANGED, this, "_on_graph_node_name_changed");
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
		PRINT_VERBOSE("Reference node for VoxelGraph previews: null");
		_debug_renderer.set_world(nullptr);
	} else {
		PRINT_VERBOSE(String("Reference node for VoxelGraph previews: {0}").format(varray(node->get_path())));
		_debug_renderer.set_world(_voxel_node->get_world().ptr());
	}
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
		const Error err = _graph_edit->connect_node(
				from_node_name, con.src.port_index, to_node_view->get_name(), con.dst.port_index);
		ERR_FAIL_COND(err != OK);
	}
}

static void update_node_view_title(VoxelGraphEditorNode *node_view, StringName node_name, String node_type_name) {
	if (node_name == StringName()) {
		node_view->set_title(node_type_name);
	} else {
		node_view->set_title(String("{0} ({1})").format(varray(node_name, node_type_name)));
	}
}

void VoxelGraphEditor::create_node_gui(uint32_t node_id) {
	// Build one GUI node

	CRASH_COND(_graph.is_null());
	const VoxelGeneratorGraph &graph = **_graph;
	const uint32_t node_type_id = graph.get_node_type_id(node_id);
	const VoxelGraphNodeDB::NodeType &node_type = VoxelGraphNodeDB::get_singleton()->get_type(node_type_id);

	const String ui_node_name = node_to_gui_name(node_id);
	ERR_FAIL_COND(_graph_edit->has_node(ui_node_name));

	VoxelGraphEditorNode *node_view = memnew(VoxelGraphEditorNode);
	node_view->set_offset(graph.get_node_gui_position(node_id) * EDSCALE);

	StringName node_name = graph.get_node_name(node_id);
	update_node_view_title(node_view, node_name, node_type.name);

	node_view->set_name(ui_node_name);
	node_view->node_id = node_id;
	node_view->connect("dragged", this, "_on_graph_node_dragged", varray(node_id));
	//node_view.resizable = true
	//node_view.rect_size = Vector2(200, 100)

	// We artificially hide output ports if the node is an output.
	// These nodes have an output for implementation reasons, some outputs can process the data like any other node.
	const bool hide_outputs = node_type.category == VoxelGraphNodeDB::CATEGORY_OUTPUT;

	const unsigned int row_count = max(node_type.inputs.size(), hide_outputs ? 0 : node_type.outputs.size());
	const Color port_color(0.4, 0.4, 1.0);

	// TODO Insert a summary so the graph would be readable without having to inspect nodes
	// However left and right slots always start from the first child item,
	// so we'd have to decouple the real slots indices from the view
	// if (node_type_id == VoxelGeneratorGraph::NODE_CLAMP) {
	// 	const float minv = graph.get_node_param(node_id, 0);
	// 	const float maxv = graph.get_node_param(node_id, 1);
	// 	Label *sl = memnew(Label);
	// 	sl->set_modulate(Color(1, 1, 1, 0.5));
	// 	sl->set_text(String("[{0}, {1}]").format(varray(minv, maxv)));
	// 	sl->set_align(Label::ALIGN_CENTER);
	// 	node_view->summary_label = sl;
	// 	node_view->add_child(sl);
	// }

	for (unsigned int i = 0; i < row_count; ++i) {
		const bool has_left = i < node_type.inputs.size();
		const bool has_right = (i < node_type.outputs.size()) && !hide_outputs;

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
			// Pass filter is required to allow tooltips to work
			label->set_mouse_filter(Control::MOUSE_FILTER_PASS);
			property_control->add_child(label);

			node_view->output_labels.push_back(label);
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

void VoxelGraphEditor::_on_graph_edit_connection_request(
		String from_node_name, int from_slot, String to_node_name, int to_slot) {
	VoxelGraphEditorNode *src_node_view = Object::cast_to<VoxelGraphEditorNode>(_graph_edit->get_node(from_node_name));
	VoxelGraphEditorNode *dst_node_view = Object::cast_to<VoxelGraphEditorNode>(_graph_edit->get_node(to_node_name));
	ERR_FAIL_COND(src_node_view == nullptr);
	ERR_FAIL_COND(dst_node_view == nullptr);

	// TODO Replace connection if one already exists

	//print("Connection attempt from ", from, ":", from_slot, " to ", to, ":", to_slot)
	if (_graph->can_connect(src_node_view->node_id, from_slot, dst_node_view->node_id, to_slot)) {
		_undo_redo->create_action(TTR("Connect Nodes"));

		_undo_redo->add_do_method(
				*_graph, "add_connection", src_node_view->node_id, from_slot, dst_node_view->node_id, to_slot);
		_undo_redo->add_do_method(
				_graph_edit, "connect_node", from_node_name, from_slot, to_node_name, to_slot);

		_undo_redo->add_undo_method(
				*_graph, "remove_connection", src_node_view->node_id, from_slot, dst_node_view->node_id, to_slot);
		_undo_redo->add_undo_method(_graph_edit, "disconnect_node", from_node_name, from_slot, to_node_name, to_slot);

		_undo_redo->commit_action();
	}
}

void VoxelGraphEditor::_on_graph_edit_disconnection_request(
		String from_node_name, int from_slot, String to_node_name, int to_slot) {
	VoxelGraphEditorNode *src_node_view = Object::cast_to<VoxelGraphEditorNode>(_graph_edit->get_node(from_node_name));
	VoxelGraphEditorNode *dst_node_view = Object::cast_to<VoxelGraphEditorNode>(_graph_edit->get_node(to_node_name));
	ERR_FAIL_COND(src_node_view == nullptr);
	ERR_FAIL_COND(dst_node_view == nullptr);

	_undo_redo->create_action(TTR("Disconnect Nodes"));

	_undo_redo->add_do_method(
			*_graph, "remove_connection", src_node_view->node_id, from_slot, dst_node_view->node_id, to_slot);
	_undo_redo->add_do_method(_graph_edit, "disconnect_node", from_node_name, from_slot, to_node_name, to_slot);

	_undo_redo->add_undo_method(
			*_graph, "add_connection", src_node_view->node_id, from_slot, dst_node_view->node_id, to_slot);
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
				_undo_redo->add_undo_method(*_graph, "add_connection",
						con.src.node_id, con.src.port_index, con.dst.node_id, con.dst.port_index);

				const String src_node_name = node_to_gui_name(con.src.node_id);
				const String dst_node_name = node_to_gui_name(con.dst.node_id);
				_undo_redo->add_undo_method(_graph_edit, "connect_node",
						src_node_name, con.src.port_index, dst_node_name, con.dst.port_index);
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
	// I haven't the faintest idea how VisualScriptEditor magically makes this work,
	// neither using `create_action` nor `commit_action`.
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

void reset_modulates(GraphEdit &graph_edit) {
	for (int child_index = 0; child_index < graph_edit.get_child_count(); ++child_index) {
		VoxelGraphEditorNode *node_view =
				Object::cast_to<VoxelGraphEditorNode>(graph_edit.get_child(child_index));
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

	uint64_t time_before = OS::get_singleton()->get_ticks_usec();

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

	uint64_t time_taken = OS::get_singleton()->get_ticks_usec() - time_before;
	PRINT_VERBOSE(String("Previews generated in {0} us").format(varray(time_taken)));
}

void VoxelGraphEditor::update_range_analysis_previews() {
	PRINT_VERBOSE("Updating range analysis previews");
	ERR_FAIL_COND(!_graph->is_good());

	const AABB aabb = _range_analysis_dialog->get_aabb();
	_graph->debug_analyze_range(
			Vector3i::from_floored(aabb.position), Vector3i::from_floored(aabb.position + aabb.size), true);

	const VoxelGraphRuntime::State &state = _graph->get_last_state_from_current_thread();

	const Color greyed_out_color(1, 1, 1, 0.5);

	for (int child_index = 0; child_index < _graph_edit->get_child_count(); ++child_index) {
		VoxelGraphEditorNode *node_view =
				Object::cast_to<VoxelGraphEditorNode>(_graph_edit->get_child(child_index));
		if (node_view == nullptr) {
			continue;
		}

		if (node_view->output_labels.size() == 0) {
			continue;
		}

		// Assume the node won't run for now
		// TODO Would be nice if GraphEdit's minimap would take such coloring into account...
		node_view->set_modulate(greyed_out_color);

		for (int port_index = 0; port_index < node_view->output_labels.size(); ++port_index) {
			ProgramGraph::PortLocation loc;
			loc.node_id = node_view->node_id;
			loc.port_index = port_index;
			uint32_t address;
			if (!_graph->try_get_output_port_address(loc, address)) {
				continue;
			}
			const Interval range = state.get_range(address);
			Control *label = node_view->output_labels[port_index];
			label->set_tooltip(String("Min: {0}\nMax: {1}").format(varray(range.min, range.max)));
		}
	}

	// Highlight only nodes that will actually run
	Span<const int> execution_map = VoxelGeneratorGraph::get_last_execution_map_debug_from_current_thread();
	for (unsigned int i = 0; i < execution_map.size(); ++i) {
		String node_view_path = node_to_gui_name(execution_map[i]);
		VoxelGraphEditorNode *node_view =
				Object::cast_to<VoxelGraphEditorNode>(_graph_edit->get_node(node_view_path));
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

	const Transform parent_transform = _voxel_node->get_global_transform();
	const AABB aabb = _range_analysis_dialog->get_aabb();
	_debug_renderer.begin();
	_debug_renderer.draw_box(
			parent_transform * Transform(Basis().scaled(aabb.size), aabb.position),
			VoxelDebug::ID_VOXEL_GRAPH_DEBUG_BOUNDS);
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
		if (!_graph->try_get_output_port_address(src, info.address)) {
			// Not part of the compiled result
			continue;
		}
		info.min_value = _graph->get_node_param(dst.node_id, 0);
		const float max_value = _graph->get_node_param(dst.node_id, 1);
		info.value_scale = 1.f / (max_value - info.min_value);
		previews.push_back(info);
	}

	const int preview_size_x = VoxelGraphEditorNodePreview::RESOLUTION;
	const int preview_size_y = VoxelGraphEditorNodePreview::RESOLUTION;
	const int buffer_size = preview_size_x * preview_size_y;
	std::vector<float> x_vec;
	std::vector<float> y_vec;
	std::vector<float> z_vec;
	x_vec.resize(buffer_size);
	y_vec.resize(buffer_size);
	z_vec.resize(buffer_size);

	const Vector3 min_pos(-preview_size_x / 2, -preview_size_y / 2, 0);
	const Vector3 max_pos = min_pos + Vector3(preview_size_x, preview_size_x, 0);

	{
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
	}

	_graph->generate_set(
			Span<float>(x_vec, 0, x_vec.size()),
			Span<float>(y_vec, 0, y_vec.size()),
			Span<float>(z_vec, 0, z_vec.size()));

	const VoxelGraphRuntime::State &last_state = VoxelGeneratorGraph::get_last_state_from_current_thread();

	for (size_t preview_index = 0; preview_index < previews.size(); ++preview_index) {
		PreviewInfo &info = previews[preview_index];

		const VoxelGraphRuntime::Buffer &buffer = last_state.get_buffer(info.address);

		Image &im = **info.control->get_image();
		ERR_FAIL_COND(im.get_width() * im.get_height() != static_cast<int>(buffer.size));

		im.lock();

		unsigned int i = 0;
		for (int y = 0; y < im.get_height(); ++y) {
			for (int x = 0; x < im.get_width(); ++x) {
				const float v = buffer.data[i];
				const float g = clamp((v - info.min_value) * info.value_scale, 0.f, 1.f);
				im.set_pixel(x, im.get_height() - y - 1, Color(g, g, g));
				++i;
			}
		}

		im.unlock();

		info.control->update_texture();
	}
}

void VoxelGraphEditor::clear_range_analysis_tooltips() {
	for (int child_index = 0; child_index < _graph_edit->get_child_count(); ++child_index) {
		VoxelGraphEditorNode *node_view =
				Object::cast_to<VoxelGraphEditorNode>(_graph_edit->get_child(child_index));
		if (node_view == nullptr) {
			continue;
		}
		for (int i = 0; i < node_view->output_labels.size(); ++i) {
			Control *oc = node_view->output_labels[i];
			oc->set_tooltip("");
		}
	}
}

void VoxelGraphEditor::schedule_preview_update() {
	_time_before_preview_update = 0.5f;
}

void VoxelGraphEditor::_on_graph_changed() {
	schedule_preview_update();
}

void VoxelGraphEditor::_on_graph_node_name_changed(int node_id) {
	StringName node_name = _graph->get_node_name(node_id);

	const uint32_t node_type_id = _graph->get_node_type_id(node_id);
	String node_type_name = VoxelGraphNodeDB::get_singleton()->get_type(node_type_id).name;

	const String ui_node_name = node_to_gui_name(node_id);
	VoxelGraphEditorNode *node_view = Object::cast_to<VoxelGraphEditorNode>(_graph_edit->get_node(ui_node_name));

	update_node_view_title(node_view, node_name, node_type_name);
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
	_range_analysis_dialog->popup_centered_minsize();
}

void VoxelGraphEditor::_on_range_analysis_toggled(bool enabled) {
	schedule_preview_update();
	update_range_analysis_gizmo();
}

void VoxelGraphEditor::_on_range_analysis_area_changed() {
	schedule_preview_update();
	update_range_analysis_gizmo();
}

void VoxelGraphEditor::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_on_graph_edit_gui_input", "event"), &VoxelGraphEditor::_on_graph_edit_gui_input);
	ClassDB::bind_method(
			D_METHOD("_on_graph_edit_connection_request", "from_node_name", "from_slot", "to_node_name", "to_slot"),
			&VoxelGraphEditor::_on_graph_edit_connection_request);
	ClassDB::bind_method(
			D_METHOD("_on_graph_edit_disconnection_request", "from_node_name", "from_slot", "to_node_name", "to_slot"),
			&VoxelGraphEditor::_on_graph_edit_disconnection_request);
	ClassDB::bind_method(D_METHOD("_on_graph_edit_delete_nodes_request"),
			&VoxelGraphEditor::_on_graph_edit_delete_nodes_request);
	ClassDB::bind_method(D_METHOD("_on_graph_edit_node_selected"), &VoxelGraphEditor::_on_graph_edit_node_selected);
	ClassDB::bind_method(D_METHOD("_on_graph_edit_node_unselected"), &VoxelGraphEditor::_on_graph_edit_node_unselected);
	ClassDB::bind_method(D_METHOD("_on_graph_node_dragged", "from", "to", "id"),
			&VoxelGraphEditor::_on_graph_node_dragged);
	ClassDB::bind_method(D_METHOD("_on_context_menu_id_pressed", "id"), &VoxelGraphEditor::_on_context_menu_id_pressed);
	ClassDB::bind_method(D_METHOD("_on_graph_changed"), &VoxelGraphEditor::_on_graph_changed);
	ClassDB::bind_method(D_METHOD("_on_graph_node_name_changed"), &VoxelGraphEditor::_on_graph_node_name_changed);
	ClassDB::bind_method(D_METHOD("_on_update_previews_button_pressed"),
			&VoxelGraphEditor::_on_update_previews_button_pressed);
	ClassDB::bind_method(D_METHOD("_on_profile_button_pressed"), &VoxelGraphEditor::_on_profile_button_pressed);
	ClassDB::bind_method(D_METHOD("_on_analyze_range_button_pressed"),
			&VoxelGraphEditor::_on_analyze_range_button_pressed);
	ClassDB::bind_method(D_METHOD("_on_range_analysis_toggled", "enabled"),
			&VoxelGraphEditor::_on_range_analysis_toggled);
	ClassDB::bind_method(D_METHOD("_on_range_analysis_area_changed"),
			&VoxelGraphEditor::_on_range_analysis_area_changed);

	ClassDB::bind_method(D_METHOD("_check_nothing_selected"), &VoxelGraphEditor::_check_nothing_selected);

	ClassDB::bind_method(D_METHOD("create_node_gui", "node_id"), &VoxelGraphEditor::create_node_gui);
	ClassDB::bind_method(D_METHOD("remove_node_gui", "node_name"), &VoxelGraphEditor::remove_node_gui);
	ClassDB::bind_method(D_METHOD("set_node_position", "node_id", "offset"), &VoxelGraphEditor::set_node_position);

	ADD_SIGNAL(MethodInfo(SIGNAL_NODE_SELECTED, PropertyInfo(Variant::INT, "node_id")));
	ADD_SIGNAL(MethodInfo(SIGNAL_NOTHING_SELECTED, PropertyInfo(Variant::INT, "nothing_selected")));
}
