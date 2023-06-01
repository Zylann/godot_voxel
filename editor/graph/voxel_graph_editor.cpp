#include "voxel_graph_editor.h"
#include "../../constants/voxel_string_names.h"
#include "../../generators/graph/node_type_db.h"
#include "../../generators/graph/voxel_generator_graph.h"
#include "../../terrain/voxel_node.h"
#include "../../util/godot/classes/button.h"
#include "../../util/godot/classes/canvas_item.h"
#include "../../util/godot/classes/check_box.h"
#include "../../util/godot/classes/control.h"
#include "../../util/godot/classes/editor_file_dialog.h"
#include "../../util/godot/classes/editor_quick_open.h"
#include "../../util/godot/classes/graph_edit.h"
#include "../../util/godot/classes/h_box_container.h"
#include "../../util/godot/classes/input_event_mouse_button.h"
#include "../../util/godot/classes/input_event_mouse_motion.h"
#include "../../util/godot/classes/label.h"
#include "../../util/godot/classes/menu_button.h"
#include "../../util/godot/classes/node.h"
#include "../../util/godot/classes/option_button.h"
#include "../../util/godot/classes/popup_menu.h"
#include "../../util/godot/classes/resource_loader.h"
#include "../../util/godot/classes/scene_tree.h"
#include "../../util/godot/classes/time.h"
#include "../../util/godot/classes/world_3d.h"
#include "../../util/godot/core/array.h"
#include "../../util/godot/core/callable.h"
#include "../../util/godot/core/input_enums.h"
#include "../../util/godot/core/mouse_button.h"
#include "../../util/godot/editor_scale.h"
#include "../../util/godot/funcs.h"
#include "../../util/log.h"
#include "../../util/macros.h"
#include "../../util/math/conv.h"
#include "../../util/profiling.h"
#include "../../util/string_funcs.h"
#include "voxel_graph_editor_node.h"
#include "voxel_graph_editor_node_preview.h"
#include "voxel_graph_editor_shader_dialog.h"
#include "voxel_range_analysis_dialog.h"

namespace zylann::voxel {

using namespace pg;

const char *VoxelGraphEditor::SIGNAL_NODE_SELECTED = "node_selected";
const char *VoxelGraphEditor::SIGNAL_NOTHING_SELECTED = "nothing_selected";
const char *VoxelGraphEditor::SIGNAL_NODES_DELETED = "nodes_deleted";
const char *VoxelGraphEditor::SIGNAL_REGENERATE_REQUESTED = "regenerate_requested";
const char *VoxelGraphEditor::SIGNAL_POPOUT_REQUESTED = "popout_requested";

enum ToolbarMenuIDs {
	MENU_UPDATE_PREVIEWS = 0,
	MENU_PROFILE,
	MENU_ANALYZE_RANGE,
	MENU_LIVE_UPDATE,
	MENU_PREVIEW_AXES,
	MENU_PREVIEW_AXES_XY,
	MENU_PREVIEW_AXES_XZ,
	MENU_PREVIEW_RESET_LOCATION,
	MENU_GENERATE_SHADER
};

enum ContextMenuSpecialIDs {
	// Preceding IDs are node types
	CONTEXT_MENU_FUNCTION_BROWSE = VoxelGraphFunction::NODE_TYPE_COUNT,
	CONTEXT_MENU_FUNCTION_QUICK_OPEN,
	CONTEXT_MENU_ID_MAX
};

static NodePath to_node_path(const StringName &sn) {
	return NodePath(String(sn));
}

VoxelGraphEditor::VoxelGraphEditor() {
	VBoxContainer *vbox_container = memnew(VBoxContainer);
	vbox_container->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);

	{
		HBoxContainer *toolbar = memnew(HBoxContainer);

		{
			MenuButton *menu_button = memnew(MenuButton);
			menu_button->set_text(ZN_TTR("Graph"));
			menu_button->set_switch_on_hover(true);

			PopupMenu *popup_menu = menu_button->get_popup();
			popup_menu->add_item(ZN_TTR("Generate Shader"), MENU_GENERATE_SHADER);

			popup_menu->connect("id_pressed", ZN_GODOT_CALLABLE_MP(this, VoxelGraphEditor, _on_menu_id_pressed));

			toolbar->add_child(menu_button);
			_graph_menu_button = menu_button;
		}
		{
			MenuButton *menu_button = memnew(MenuButton);
			menu_button->set_text(ZN_TTR("Debug"));
			menu_button->set_switch_on_hover(true);

			PopupMenu *popup_menu = menu_button->get_popup();
			popup_menu->add_item(ZN_TTR("Update Previews"), MENU_UPDATE_PREVIEWS);
			popup_menu->add_item(ZN_TTR("Profile"), MENU_PROFILE);
			popup_menu->add_item(ZN_TTR("Analyze Range..."), MENU_ANALYZE_RANGE);

			{
				const int idx = popup_menu->get_item_count();
				popup_menu->add_check_item(ZN_TTR("Live Update"), MENU_LIVE_UPDATE);
				popup_menu->set_item_tooltip(
						idx, ZN_TTR("Automatically re-generate the terrain when the generator is modified"));
				popup_menu->set_item_checked(idx, _live_update_enabled);
			}

			{
				PopupMenu *sub_menu = memnew(PopupMenu);
				sub_menu->set_name("PreviewAxisMenu");
				sub_menu->add_radio_check_item("XY", MENU_PREVIEW_AXES_XY);
				sub_menu->add_radio_check_item("XZ", MENU_PREVIEW_AXES_XZ);
				sub_menu->add_item("Reset location", MENU_PREVIEW_RESET_LOCATION);
				sub_menu->connect("id_pressed", ZN_GODOT_CALLABLE_MP(this, VoxelGraphEditor, _on_menu_id_pressed));
				popup_menu->add_child(sub_menu);
				popup_menu->add_submenu_item(ZN_TTR("Preview Axes"), sub_menu->get_name(), MENU_PREVIEW_AXES);
				_preview_axes_menu = sub_menu;
				update_preview_axes_menu();
			}

			popup_menu->connect("id_pressed", ZN_GODOT_CALLABLE_MP(this, VoxelGraphEditor, _on_menu_id_pressed));

			toolbar->add_child(menu_button);
			_debug_menu_button = menu_button;
		}

		_profile_label = memnew(Label);
		toolbar->add_child(_profile_label);

		_compile_result_label = memnew(Label);
		_compile_result_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		_compile_result_label->set_clip_text(true);
		_compile_result_label->hide();
		toolbar->add_child(_compile_result_label);

		Control *spacer = memnew(Control);
		spacer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		toolbar->add_child(spacer);

		_pin_button = memnew(Button);
		_pin_button->set_flat(true);
		_pin_button->set_toggle_mode(true);
		_pin_button->set_tooltip_text(ZN_TTR("Pin VoxelGraphEditor"));
		toolbar->add_child(_pin_button);

		_popout_button = memnew(Button);
		_popout_button->set_flat(true);
		_popout_button->set_tooltip_text(ZN_TTR("Pop-out as separate window"));
		_popout_button->connect("pressed", ZN_GODOT_CALLABLE_MP(this, VoxelGraphEditor, _on_popout_button_pressed));
		toolbar->add_child(_popout_button);

		vbox_container->add_child(toolbar);
	}

	_graph_edit = memnew(GraphEdit);
	_graph_edit->set_anchors_preset(Control::PRESET_FULL_RECT);
	_graph_edit->set_right_disconnects(true);
	// TODO Performance: sorry, had to turn off AA because Godot's current implementation is incredibly slow.
	// It slows down the editor a lot when a graph has lots of connections. Because despite Godot 4 now supporting
	// 2D MSAA, it still relies on a fake AA method which draws 9 times the same line and allocates
	// memory (malloc) on the fly. See `RendererCanvasCull::canvas_item_add_polyline`.
	// 2D MSAA also is only exposed in Project Settings, which does not apply to editor UIs... (and shouldn't, but there
	// should be a setting in Editor Settings).
	_graph_edit->set_connection_lines_antialiased(false);
	_graph_edit->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	_graph_edit->connect("gui_input", ZN_GODOT_CALLABLE_MP(this, VoxelGraphEditor, _on_graph_edit_gui_input));
	_graph_edit->connect(
			"connection_request", ZN_GODOT_CALLABLE_MP(this, VoxelGraphEditor, _on_graph_edit_connection_request));
	_graph_edit->connect(
			"delete_nodes_request", ZN_GODOT_CALLABLE_MP(this, VoxelGraphEditor, _on_graph_edit_delete_nodes_request));
	_graph_edit->connect("disconnection_request",
			ZN_GODOT_CALLABLE_MP(this, VoxelGraphEditor, _on_graph_edit_disconnection_request));
	_graph_edit->connect("node_selected", ZN_GODOT_CALLABLE_MP(this, VoxelGraphEditor, _on_graph_edit_node_selected));
	_graph_edit->connect(
			"node_deselected", ZN_GODOT_CALLABLE_MP(this, VoxelGraphEditor, _on_graph_edit_node_deselected));
	vbox_container->add_child(_graph_edit);

	add_child(vbox_container);

	_context_menu = memnew(PopupMenu);
	FixedArray<PopupMenu *, CATEGORY_COUNT> category_menus;
	for (unsigned int i = 0; i < category_menus.size(); ++i) {
		if (i == CATEGORY_RELAY) {
			category_menus[i] = nullptr;
			_context_menu->add_item("Relay", VoxelGraphFunction::NODE_RELAY);
			continue;
		}
		if (i == CATEGORY_CONSTANT) {
			continue;
		}
		String name = get_category_name(Category(i));
		PopupMenu *menu = memnew(PopupMenu);
		menu->set_name(name);
		menu->connect("id_pressed", ZN_GODOT_CALLABLE_MP(this, VoxelGraphEditor, _on_context_menu_id_pressed));
		_context_menu->add_child(menu);
		_context_menu->add_submenu_item(name, name, i);
		category_menus[i] = menu;
	}
	// TODO Usability: have CustomInput and CustomOutput subcategories based on I/O definitions, + a "new" option for
	// unbound
	for (int i = 0; i < NodeTypeDB::get_singleton().get_type_count(); ++i) {
		if (i == VoxelGraphFunction::NODE_RELAY) {
			continue;
		}

		const NodeType &node_type = NodeTypeDB::get_singleton().get_type(i);

		if (i == VoxelGraphFunction::NODE_FUNCTION) {
			PopupMenu *menu = category_menus[node_type.category];
			ZN_ASSERT(menu != nullptr);
			menu->add_item(ZN_TTR("Browse..."), CONTEXT_MENU_FUNCTION_BROWSE);
#ifdef ZN_GODOT
			menu->add_item(TTR("Quick Open..."), CONTEXT_MENU_FUNCTION_QUICK_OPEN);
#endif
		} else if (i == VoxelGraphFunction::NODE_CONSTANT) {
			// Exceptionally keep the constant node into the input category in the editor. Internally, "Input" means the
			// node is an entry point of the graph, like a function parameter. Constants are not considered as such, and
			// won't work as inputs in graph functions. They have their own category internally, but it still makes
			// sense to have them in the "input" sub-menu in the editor.
			PopupMenu *menu = category_menus[CATEGORY_INPUT];
			ZN_ASSERT(menu != nullptr);
			menu->add_item(node_type.name, i);

		} else {
			PopupMenu *menu = category_menus[node_type.category];
			ZN_ASSERT(menu != nullptr);
			menu->add_item(node_type.name, i);
		}
	}
	_context_menu->connect("id_pressed", ZN_GODOT_CALLABLE_MP(this, VoxelGraphEditor, _on_context_menu_id_pressed));
	_context_menu->hide();
	add_child(_context_menu);

	_range_analysis_dialog = memnew(VoxelRangeAnalysisDialog);
	_range_analysis_dialog->connect(
			"analysis_toggled", ZN_GODOT_CALLABLE_MP(this, VoxelGraphEditor, _on_range_analysis_toggled));
	_range_analysis_dialog->connect(
			"area_changed", ZN_GODOT_CALLABLE_MP(this, VoxelGraphEditor, _on_range_analysis_area_changed));
	add_child(_range_analysis_dialog);

	_shader_dialog = memnew(VoxelGraphEditorShaderDialog);
	add_child(_shader_dialog);

	_function_file_dialog = memnew(EditorFileDialog);
	_function_file_dialog->set_access(EditorFileDialog::ACCESS_RESOURCES);
	_function_file_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
	// TODO Usability: there is no way to limit a file dialog to a specific TYPE of resource, only file extensions. So
	// it's not useful because text resources are almost all using `.tres`...
	_function_file_dialog->add_filter("*.tres", ZN_TTR("Text Resource"));
	_function_file_dialog->add_filter("*.res", ZN_TTR("Binary Resource"));
	_function_file_dialog->connect(
			"file_selected", ZN_GODOT_CALLABLE_MP(this, VoxelGraphEditor, _on_function_file_dialog_file_selected));
	add_child(_function_file_dialog);

	// TODO GDX: EditorQuickOpen is not exposed to extensions
#ifdef ZN_GODOT
	_function_quick_open_dialog = memnew(EditorQuickOpen);
	_function_quick_open_dialog->connect(
			"quick_open", ZN_GODOT_CALLABLE_MP(this, VoxelGraphEditor, _on_function_quick_open_dialog_quick_open));
	add_child(_function_quick_open_dialog);
#endif

	update_buttons_availability();
}

void VoxelGraphEditor::set_generator(Ref<VoxelGeneratorGraph> generator) {
	if (_generator == generator) {
		return;
	}

	_generator = generator;

	if (_generator.is_valid()) {
		Ref<VoxelGraphFunction> graph = generator->get_main_function();

		// Load a default preset when creating new graphs.
		// TODO Downside is, an empty graph cannot be seen.
		// But Godot doesnt let us know if the resource has been created from the inspector or not
		if (graph->get_nodes_count() == 0) {
			_generator->load_plane_preset();
		}

		set_graph(graph);

	} else {
		set_graph(Ref<VoxelGraphFunction>());
	}

	schedule_preview_update();

	update_buttons_availability();
}

void VoxelGraphEditor::set_graph(Ref<VoxelGraphFunction> graph) {
	if (_graph == graph) {
		return;
	}

	if (_graph.is_valid()) {
		_graph->disconnect(VoxelStringNames::get_singleton().changed,
				ZN_GODOT_CALLABLE_MP(this, VoxelGraphEditor, _on_graph_changed));
		_graph->disconnect(VoxelGeneratorGraph::SIGNAL_NODE_NAME_CHANGED,
				ZN_GODOT_CALLABLE_MP(this, VoxelGraphEditor, _on_graph_node_name_changed));
	}

	_graph = graph;

	if (_graph.is_valid()) {
		_graph->connect(VoxelStringNames::get_singleton().changed,
				ZN_GODOT_CALLABLE_MP(this, VoxelGraphEditor, _on_graph_changed));
		_graph->connect(VoxelGeneratorGraph::SIGNAL_NODE_NAME_CHANGED,
				ZN_GODOT_CALLABLE_MP(this, VoxelGraphEditor, _on_graph_node_name_changed));

	} else {
		_graph = Ref<VoxelGraphFunction>();
	}

	_debug_renderer.clear();

	build_gui_from_graph();

	if (_graph.is_valid()) {
		update_functions();
	}

	// schedule_preview_update();
}

Ref<VoxelGraphFunction> VoxelGraphEditor::get_graph() const {
	return _graph;
}

void VoxelGraphEditor::set_undo_redo(EditorUndoRedoManager *undo_redo) {
	_undo_redo = undo_redo;
}

EditorUndoRedoManager *VoxelGraphEditor::get_undo_redo() const {
	return _undo_redo;
}

void VoxelGraphEditor::set_voxel_node(VoxelNode *node) {
	_voxel_node = node;
	if (_voxel_node == nullptr) {
		ZN_PRINT_VERBOSE("Reference node for VoxelGraph gizmos: null");
		_debug_renderer.set_world(nullptr);
	} else {
		ZN_PRINT_VERBOSE(format("Reference node for VoxelGraph gizmos: {}", String(node->get_path())));
		_debug_renderer.set_world(_voxel_node->get_world_3d().ptr());
	}
}

#ifdef ZN_GODOT_EXTENSION
void VoxelGraphEditor::_process(double delta) {
	process(delta);
}
#endif

void VoxelGraphEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_INTERNAL_PROCESS:
			process(get_process_delta_time());
			break;

		case NOTIFICATION_VISIBILITY_CHANGED:
			set_process_internal(is_visible());
			break;

		case NOTIFICATION_THEME_CHANGED: {
			const VoxelStringNames &sn = VoxelStringNames::get_singleton();
			set_button_icon(*_pin_button, get_theme_icon(sn.Pin, sn.EditorIcons));
			set_button_icon(*_popout_button, get_theme_icon(sn.ExternalLink, sn.EditorIcons));
		} break;
	}
}

void VoxelGraphEditor::process(float delta) {
	if (_time_before_preview_update > 0.f) {
		_time_before_preview_update -= delta;
		if (_time_before_preview_update < 0.f) {
			update_previews(true);
		}
	}

	// I decided to do by polling to display some things on graph nodes, so all the code is here and there is no faffing
	// around with signals.
	if (_graph.is_valid() && is_visible_in_tree()) {
		for (int child_node_index = 0; child_node_index < _graph_edit->get_child_count(); ++child_node_index) {
			Node *node = _graph_edit->get_child(child_node_index);
			VoxelGraphEditorNode *node_view = Object::cast_to<VoxelGraphEditorNode>(node);
			if (node_view != nullptr) {
				node_view->poll(**_graph);
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

	const VoxelGraphFunction &graph = **_graph;

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
		VoxelGraphEditorNode *to_node_view = get_node_typed<VoxelGraphEditorNode>(*_graph_edit, NodePath(to_node_name));
		ERR_FAIL_COND(to_node_view == nullptr);
		const Error err = _graph_edit->connect_node(
				from_node_name, con.src.port_index, to_node_view->get_name(), con.dst.port_index);

		ERR_FAIL_COND(err != OK);
	}
}

void VoxelGraphEditor::create_node_gui(uint32_t node_id) {
	// Build one GUI node

	CRASH_COND(_graph.is_null());
	// Checking because when creating a new node, UndoRedo methods carry on even if one fails
	ERR_FAIL_COND(_graph->has_node(node_id) == false);

	const String ui_node_name = node_to_gui_name(node_id);
	ERR_FAIL_COND(_graph_edit->has_node(ui_node_name));

	VoxelGraphEditorNode *node_view = VoxelGraphEditorNode::create(**_graph, node_id);
	node_view->set_name(ui_node_name);

#if defined(ZN_GODOT)
	node_view->connect("dragged", ZN_GODOT_CALLABLE_MP(this, VoxelGraphEditor, _on_graph_node_dragged).bind(node_id));
	node_view->connect(
			"resize_request", ZN_GODOT_CALLABLE_MP(this, VoxelGraphEditor, _on_node_resize_request).bind(node_id));
#elif defined(ZN_GODOT_EXTENSION)
	// TODO GDX: `Callable::bind()` isn't implemented in GodotCpp
	ZN_PRINT_ERROR("`Callable::bind()` isn't working in GodotCpp! Can't handle dragging and resizing nodes action with "
				   "UndoRedo.");
#endif

	VoxelGraphEditorNodePreview *preview = node_view->get_preview();
	if (preview != nullptr) {
		preview->update_display_settings(**_graph, node_id);

		preview->connect("gui_input", ZN_GODOT_CALLABLE_MP(this, VoxelGraphEditor, _on_graph_node_preview_gui_input));
	}

	_graph_edit->add_child(node_view);
}

void remove_connections_from_and_to(GraphEdit &graph_edit, StringName node_name) {
	// Get copy of connection list
	std::vector<GodotGraphEditConnection> connections;
	get_graph_edit_connections(graph_edit, connections);

	for (const GodotGraphEditConnection &con : connections) {
		if (con.from == node_name || con.to == node_name) {
			graph_edit.disconnect_node(con.from, con.from_port, con.to, con.to_port);
		}
	}
}

void VoxelGraphEditor::remove_node_gui(StringName gui_node_name) {
	// Remove connections from the UI, because GraphNode doesn't do it...
	remove_connections_from_and_to(*_graph_edit, gui_node_name);

	Node *node_view = get_node_typed<Node>(*_graph_edit, to_node_path(gui_node_name));
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

void VoxelGraphEditor::update_node_layout(uint32_t node_id) {
	ERR_FAIL_COND(_graph.is_null());

	GraphEdit &graph_edit = *_graph_edit;
	const String view_name = node_to_gui_name(node_id);
	VoxelGraphEditorNode *view = get_node_typed<VoxelGraphEditorNode>(graph_edit, view_name);
	ERR_FAIL_COND(view == nullptr);

	// Remove all GUI connections going to the node

	std::vector<GodotGraphEditConnection> old_connections;
	get_graph_edit_connections(graph_edit, old_connections);

	for (const GodotGraphEditConnection &con : old_connections) {
		const NodePath to = to_node_path(con.to);
		const VoxelGraphEditorNode *to_view = get_node_typed<VoxelGraphEditorNode>(graph_edit, to);
		if (to_view == nullptr) {
			continue;
		}
		if (to_view == view) {
			graph_edit.disconnect_node(con.from, con.from_port, con.to, con.to_port);
		}
	}

	// Update node layout

	view->update_layout(**_graph);

	// TODO What about output connections?
	// Currently assuming there is always only one for expression nodes, therefore it might be ok?

	// Add connections back by reading the graph

	// TODO Optimize: the graph stores an adjacency list, we could use that
	std::vector<ProgramGraph::Connection> connections;
	_graph->get_connections(connections);

	for (size_t i = 0; i < connections.size(); ++i) {
		const ProgramGraph::Connection &con = connections[i];

		if (con.dst.node_id == node_id) {
			graph_edit.connect_node(node_to_gui_name(con.src.node_id), con.src.port_index,
					node_to_gui_name(con.dst.node_id), con.dst.port_index);
		}
	}
}

void VoxelGraphEditor::update_node_comment(uint32_t node_id) {
	ERR_FAIL_COND(_graph.is_null());

	GraphEdit &graph_edit = *_graph_edit;
	const String view_name = node_to_gui_name(node_id);
	VoxelGraphEditorNode *view = get_node_typed<VoxelGraphEditorNode>(graph_edit, view_name);
	ERR_FAIL_COND(view == nullptr);

	view->update_comment_text(**_graph);
}

bool VoxelGraphEditor::is_pinned_hint() const {
	return _pin_button->is_pressed();
}

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
			if (mb->get_button_index() == godot::MOUSE_BUTTON_RIGHT) {
				_click_position = mb->get_position();
				// Careful with how the position is computed, some users have multiple monitors but OSes handle it in
				// different ways, either with two desktops or one expanded desktop. This affects mouse positions.
				// I took example on context menus in `filesystem_dock.cpp`.
				_context_menu->set_position(_graph_edit->get_screen_position() + mb->get_position());
				_context_menu->popup();
			}
		}
	}
}

void VoxelGraphEditor::_on_graph_edit_connection_request(
		String from_node_name, int from_slot, String to_node_name, int to_slot) {
	//
	VoxelGraphEditorNode *src_node_view = get_node_typed<VoxelGraphEditorNode>(*_graph_edit, from_node_name);
	VoxelGraphEditorNode *dst_node_view = get_node_typed<VoxelGraphEditorNode>(*_graph_edit, to_node_name);
	ERR_FAIL_COND(src_node_view == nullptr);
	ERR_FAIL_COND(dst_node_view == nullptr);

	const uint32_t src_node_id = src_node_view->get_generator_node_id();
	const uint32_t dst_node_id = dst_node_view->get_generator_node_id();

	// print("Connection attempt from ", from, ":", from_slot, " to ", to, ":", to_slot)

	if (!_graph->is_valid_connection(src_node_id, from_slot, dst_node_id, to_slot)) {
		ZN_PRINT_VERBOSE("Connection is invalid");
		return;
	}

	_undo_redo->create_action(ZN_TTR("Connect Nodes"));

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
	VoxelGraphEditorNode *src_node_view = get_node_typed<VoxelGraphEditorNode>(*_graph_edit, from_node_name);
	VoxelGraphEditorNode *dst_node_view = get_node_typed<VoxelGraphEditorNode>(*_graph_edit, to_node_name);
	ERR_FAIL_COND(src_node_view == nullptr);
	ERR_FAIL_COND(dst_node_view == nullptr);

	const uint32_t src_node_id = src_node_view->get_generator_node_id();
	const uint32_t dst_node_id = dst_node_view->get_generator_node_id();

	_undo_redo->create_action(ZN_TTR("Disconnect Nodes"));

	_undo_redo->add_do_method(*_graph, "remove_connection", src_node_id, from_slot, dst_node_id, to_slot);
	_undo_redo->add_do_method(_graph_edit, "disconnect_node", from_node_name, from_slot, to_node_name, to_slot);

	_undo_redo->add_undo_method(*_graph, "add_connection", src_node_id, from_slot, dst_node_id, to_slot);
	_undo_redo->add_undo_method(_graph_edit, "connect_node", from_node_name, from_slot, to_node_name, to_slot);

	_undo_redo->commit_action();
}

#if defined(ZN_GODOT)
void VoxelGraphEditor::_on_graph_edit_delete_nodes_request(TypedArray<StringName> node_names) {
#elif defined(ZN_GODOT_EXTENSION)
void VoxelGraphEditor::_on_graph_edit_delete_nodes_request(Array node_names) {
#endif
	std::vector<VoxelGraphEditorNode *> to_erase;

	// The `node_names` argument is the result of Godot issue #61112. While it is less convenient than just getting
	// the nodes themselves, it also has the downside of being always empty if you choose to not show "close" buttons
	// on every graph node corner, even if you have nodes selected. That behavior was even documented. Go figure.
	// So... I keep doing it the old way.
	for (int i = 0; i < _graph_edit->get_child_count(); ++i) {
		VoxelGraphEditorNode *node_view = Object::cast_to<VoxelGraphEditorNode>(_graph_edit->get_child(i));
		if (node_view != nullptr) {
			if (node_view->is_selected()) {
				to_erase.push_back(node_view);
			}
		}
	}

	_undo_redo->create_action(ZN_TTR("Delete Nodes"));

	std::vector<ProgramGraph::Connection> connections;
	_graph->get_connections(connections);

	for (size_t i = 0; i < to_erase.size(); ++i) {
		const VoxelGraphEditorNode *node_view = to_erase[i];
		const uint32_t node_id = node_view->get_generator_node_id();
		const uint32_t node_type_id = _graph->get_node_type_id(node_id);

		_undo_redo->add_do_method(*_graph, "remove_node", node_id);
		_undo_redo->add_do_method(this, "remove_node_gui", node_view->get_name());

		if (node_type_id == VoxelGraphFunction::NODE_FUNCTION) {
			Ref<VoxelGraphFunction> func = _graph->get_node_param(node_id, 0);
			_undo_redo->add_undo_method(
					*_graph, "create_function_node", func, _graph->get_node_gui_position(node_id), node_id);
		} else {
			_undo_redo->add_undo_method(
					*_graph, "create_node", node_type_id, _graph->get_node_gui_position(node_id), node_id);
		}

		// Params undo
		const size_t param_count = NodeTypeDB::get_singleton().get_type(node_type_id).params.size();
		for (size_t j = 0; j < param_count; ++j) {
			Variant param_value = _graph->get_node_param(node_id, j);
			_undo_redo->add_undo_method(*_graph, "set_node_param", node_id, ZN_SIZE_T_TO_VARIANT(j), param_value);
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

void VoxelGraphEditor::_on_menu_id_pressed(int id) {
	switch (id) {
		case MENU_ANALYZE_RANGE:
			_range_analysis_dialog->popup_centered();
			break;

		case MENU_PREVIEW_AXES_XY:
			_preview_axes = PREVIEW_AXES_XY;
			schedule_preview_update();
			update_preview_axes_menu();
			break;

		case MENU_PREVIEW_AXES_XZ:
			_preview_axes = PREVIEW_AXES_XZ;
			schedule_preview_update();
			update_preview_axes_menu();
			break;

		case MENU_PREVIEW_RESET_LOCATION:
			set_preview_transform(Vector2f(0, 0), 1.f);
			break;

		case MENU_PROFILE:
			profile();
			break;

		case MENU_UPDATE_PREVIEWS:
			update_previews(false);
			break;

		case MENU_LIVE_UPDATE: {
			_live_update_enabled = !_live_update_enabled;
			PopupMenu *menu = _debug_menu_button->get_popup();
			const int idx = menu->get_item_index(id);
			menu->set_item_checked(idx, _live_update_enabled);
		} break;

		case MENU_GENERATE_SHADER: {
			ERR_FAIL_COND(_generator.is_null());
			VoxelGenerator::ShaderSourceData sd;
			if (!_generator->get_shader_source(sd)) {
				return;
			}
			// TODO Include uniforms in that version?
			_shader_dialog->set_shader_code(sd.glsl);
			_shader_dialog->popup_centered();
		} break;

		default:
			ERR_PRINT("Unknown menu item");
			break;
	}
}

void VoxelGraphEditor::_on_graph_node_dragged(Vector2 from, Vector2 to, int id) {
	_undo_redo->create_action(ZN_TTR("Move nodes"));
	_undo_redo->add_do_method(this, "set_node_position", id, to);
	_undo_redo->add_undo_method(this, "set_node_position", id, from);
	_undo_redo->commit_action();
	// I haven't the faintest idea how VisualScriptEditor magically makes this work,
	// neither using `create_action` nor `commit_action`.
}

void VoxelGraphEditor::set_node_position(int id, Vector2 offset) {
	String node_name = node_to_gui_name(id);
	GraphNode *node_view = get_node_typed<GraphNode>(*_graph_edit, node_name);
	if (node_view != nullptr) {
		node_view->set_position_offset(offset);
	}
	// We store GUI node positions independently from editor scale, to make the graph display the same regardless of
	// monitor DPI, so we have to unapply it
	_graph->set_node_gui_position(id, offset / EDSCALE);
}

void VoxelGraphEditor::_on_node_resize_request(Vector2 new_size, int node_id) {
	const String node_view_path = node_to_gui_name(node_id);
	Node *node = get_node_typed<Node>(*_graph_edit, node_view_path);
	ZN_ASSERT_RETURN(node != nullptr);
	VoxelGraphEditorNode *node_view = Object::cast_to<VoxelGraphEditorNode>(node);
	ZN_ASSERT_RETURN(node_view != nullptr);
	ZN_ASSERT_RETURN(_graph.is_valid());

	// TODO Not sure if EDSCALE has to be unapplied in this case?
	_undo_redo->create_action(ZN_TTR("Resize Node"), UndoRedo::MERGE_ENDS);
	_undo_redo->add_do_method(this, "set_node_size", node_id, new_size);
	_undo_redo->add_do_method(_graph.ptr(), "set_node_gui_size", node_id, new_size);
	_undo_redo->add_undo_method(this, "set_node_size", node_id, node_view->get_size());
	_undo_redo->add_undo_method(_graph.ptr(), "set_node_gui_size", node_id, node_view->get_size());
	_undo_redo->commit_action();
}

void VoxelGraphEditor::set_node_size(int id, Vector2 size) {
	String node_name = node_to_gui_name(id);
	GraphNode *node_view = get_node_typed<GraphNode>(*_graph_edit, node_name);
	if (node_view != nullptr) {
		node_view->set_size(size);
	}
	// This function is used solely for the UI, since we should not pass node pointers directly to UndoRedo, they could
	// have been deleted when the Undo action is called later
	//_graph->set_node_gui_size(id, size / EDSCALE);
}

void VoxelGraphEditor::_on_graph_node_preview_gui_input(Ref<InputEvent> event) {
	Ref<InputEventMouseMotion> mm = event;
	if (mm.is_valid()) {
		// Ctrl+Drag above any preview to pan around the area they render.
		if (mm->is_command_or_control_pressed() && mm->get_button_mask().has_flag(ZN_GODOT_MouseButtonMask_MIDDLE)) {
			const Vector2 rel = mm->get_relative();
			set_preview_transform(_preview_offset - Vector2f(rel.x, -rel.y) * _preview_scale, _preview_scale);

			// Prevent panning of GraphEdit
			get_viewport()->set_input_as_handled();
		}
	}

	Ref<InputEventMouseButton> mb = event;
	if (mb.is_valid()) {
		// Ctrl+Wheel above any preview to zoom in and out the area they render.
		if (mb->is_command_or_control_pressed()) {
			const float base_factor = 1.1f;
			if (mb->get_button_index() == ZN_GODOT_MouseButton_WHEEL_UP) {
				set_preview_transform(_preview_offset, _preview_scale / base_factor);
				// Prevent panning of GraphEdit
				get_viewport()->set_input_as_handled();
			}
			if (mb->get_button_index() == ZN_GODOT_MouseButton_WHEEL_DOWN) {
				set_preview_transform(_preview_offset, _preview_scale * base_factor);
				// Prevent panning of GraphEdit
				get_viewport()->set_input_as_handled();
			}
		}
	}
}

void VoxelGraphEditor::set_preview_transform(Vector2f offset, float scale) {
	if (offset != _preview_offset || scale != _preview_scale) {
		_preview_offset = offset;
		_preview_scale = scale;
		// Update quickly
		if (_time_before_preview_update <= 0.f) {
			_time_before_preview_update = 0.1f;
		}
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

void VoxelGraphEditor::_on_context_menu_id_pressed(int id) {
	if (id == CONTEXT_MENU_FUNCTION_BROWSE) {
		_function_file_dialog->popup();
		return;
	}
#ifdef ZN_GODOT
	if (id == CONTEXT_MENU_FUNCTION_QUICK_OPEN) {
		_function_quick_open_dialog->popup_dialog(VoxelGraphFunction::get_class_static());
		return;
	}
#endif

	// Create a base node type

	const Vector2 pos = get_graph_offset_from_mouse(_graph_edit, _click_position);
	const uint32_t node_type_id = id;

	const uint32_t node_id = _graph->generate_node_id();
	const StringName node_name = node_to_gui_name(node_id);

	_undo_redo->create_action(ZN_TTR("Create Node"));
	_undo_redo->add_do_method(*_graph, "create_node", node_type_id, pos, node_id);
	_undo_redo->add_do_method(this, "create_node_gui", node_id);
	_undo_redo->add_undo_method(*_graph, "remove_node", node_id);
	_undo_redo->add_undo_method(this, "remove_node_gui", node_name);
	_undo_redo->commit_action();
}

#if defined(ZN_GODOT)
void VoxelGraphEditor::_on_graph_edit_node_selected(Node *p_node) {
#elif defined(ZN_GODOT_EXTENSION)
void VoxelGraphEditor::_on_graph_edit_node_selected(Object *p_node_o) {
	Node *p_node = Object::cast_to<Node>(p_node_o);
#endif
	VoxelGraphEditorNode *node = Object::cast_to<VoxelGraphEditorNode>(p_node);
	emit_signal(SIGNAL_NODE_SELECTED, node->get_generator_node_id());
}

#if defined(ZN_GODOT)
void VoxelGraphEditor::_on_graph_edit_node_deselected(Node *p_node) {
#elif defined(ZN_GODOT_EXTENSION)
void VoxelGraphEditor::_on_graph_edit_node_deselected(Object *p_node_o) {
	// Node *p_node = Object::cast_to<Node>(p_node_o);
#endif
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

void VoxelGraphEditor::update_previews(bool with_live_update) {
	if (_generator.is_null()) {
		return;
	}

	clear_range_analysis_tooltips();
	hide_profiling_ratios();
	reset_modulates(*_graph_edit);

	update_functions();

	const uint64_t time_before = Time::get_singleton()->get_ticks_usec();

	const pg::CompilationResult result = _generator->compile(true);
	if (!result.success) {
		ERR_PRINT(String("Voxel graph compilation failed: {0}").format(varray(result.message)));

		_compile_result_label->set_text(result.message);
		_compile_result_label->set_tooltip_text(result.message);
		_compile_result_label->set_modulate(Color(1, 0.3, 0.1));
		_compile_result_label->show();

		if (result.node_id >= 0) {
			String node_view_path = node_to_gui_name(result.node_id);
			VoxelGraphEditorNode *node_view = get_node_typed<VoxelGraphEditorNode>(*_graph_edit, node_view_path);
			node_view->set_modulate(Color(1, 0.3, 0.1));
		}
		return;

	} else {
		_compile_result_label->hide();
	}

	if (!_generator->is_good()) {
		return;
	}
	// We assume no other thread will try to modify the graph and compile something not good

	update_slice_previews();

	if (_range_analysis_dialog->is_analysis_enabled()) {
		update_range_analysis_previews();
	}

	const uint64_t time_taken = Time::get_singleton()->get_ticks_usec() - time_before;
	ZN_PRINT_VERBOSE(format("Previews generated in {} us", time_taken));

	if (_live_update_enabled && with_live_update) {
		// Check if the graph changed in a way that actually changes the output,
		// because re-generating all voxels is expensive.
		// Note, sub-resouces can be involved, not just node connections and properties.
		const uint64_t hash = _graph->get_output_graph_hash();

		if (hash != _last_output_graph_hash) {
			_last_output_graph_hash = hash;

			// Not calling into `_voxel_node` directly because the editor could be pinned and the terrain not actually
			// selected. In this situation the plugin may reset the node to null. But it is desirable for terrains
			// using the current graph to update if they are in the edited scene, so this may be delegated to the editor
			// plugin. There isn't enough context from here to do this cleanly.
			emit_signal(SIGNAL_REGENERATE_REQUESTED);
		}
	}
}

void VoxelGraphEditor::update_range_analysis_previews() {
	ZN_PRINT_VERBOSE("Updating range analysis previews");
	ERR_FAIL_COND(_generator.is_null());
	ERR_FAIL_COND(!_generator->is_good());

	const AABB aabb = _range_analysis_dialog->get_aabb();
	_generator->debug_analyze_range(
			math::floor_to_int(aabb.position), math::floor_to_int(aabb.position + aabb.size), true);

	const pg::Runtime::State &state = _generator->get_last_state_from_current_thread();

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

		node_view->update_range_analysis_tooltips(**_generator, state);
	}

	// Highlight only nodes that will actually run.
	// Note, some nodes can appear twice in this map due to internal expansion.
	Span<const uint32_t> execution_map = VoxelGeneratorGraph::get_last_execution_map_debug_from_current_thread();
	for (unsigned int i = 0; i < execution_map.size(); ++i) {
		const uint32_t node_id = execution_map[i];
		// Some returned nodes might not be in the user-facing graph because they get generated during compilation
		if (!_graph->has_node(node_id)) {
			ZN_PRINT_VERBOSE(
					format("Ignoring node {} from range analysis results, not present in user graph", node_id));
			continue;
		}
		const String node_view_path = node_to_gui_name(node_id);
		Node *node = get_node_typed<Node>(*_graph_edit, node_view_path);
		ZN_ASSERT_CONTINUE(node != nullptr);
		VoxelGraphEditorNode *node_view = Object::cast_to<VoxelGraphEditorNode>(node);
		ZN_ASSERT_CONTINUE(node_view != nullptr);
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
	ZN_PRINT_VERBOSE("Updating slice previews");
	ERR_FAIL_COND(!_generator->is_good());

	struct PreviewInfo {
		VoxelGraphEditorNodePreview *control;
		uint32_t address;
		uint32_t node_id;
	};

	std::vector<PreviewInfo> previews;

	// Gather preview nodes
	for (int i = 0; i < _graph_edit->get_child_count(); ++i) {
		const VoxelGraphEditorNode *node = Object::cast_to<VoxelGraphEditorNode>(_graph_edit->get_child(i));
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
		if (!_generator->try_get_output_port_address(src, info.address)) {
			// Not part of the compiled result
			continue;
		}
		info.node_id = dst.node_id;
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

		const float view_size_x = _preview_scale * float(preview_size_x);
		const float view_size_y = _preview_scale * float(preview_size_x);
		const Vector3f min_pos =
				Vector3f(-view_size_x * 0.5f + _preview_offset.x, -view_size_y * 0.5f + _preview_offset.y, 0);
		const Vector3f max_pos = min_pos + Vector3f(view_size_x, view_size_y, 0);

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
		if (_preview_axes == PREVIEW_AXES_XY) {
			y_coords = to_span(y_vec);
			z_coords = to_span(z_vec);
		} else {
			y_coords = to_span(z_vec);
			z_coords = to_span(y_vec);
		}

		_generator->generate_set(x_coords, y_coords, z_coords);
	}

	const pg::Runtime::State &last_state = VoxelGeneratorGraph::get_last_state_from_current_thread();

	// Update previews
	for (size_t preview_index = 0; preview_index < previews.size(); ++preview_index) {
		PreviewInfo &info = previews[preview_index];
		const pg::Runtime::Buffer &buffer = last_state.get_buffer(info.address);
		info.control->update_from_buffer(buffer);
		info.control->update_display_settings(**_graph, info.node_id);
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

	const uint32_t node_type_id = _graph->get_node_type_id(node_id);

	const String ui_node_name = node_to_gui_name(node_id);
	VoxelGraphEditorNode *node_view = get_node_typed<VoxelGraphEditorNode>(*_graph_edit, ui_node_name);
	ERR_FAIL_COND(node_view == nullptr);

	if (node_type_id != VoxelGraphFunction::NODE_EXPRESSION) {
		node_view->update_title(**_graph);
	}
}

void VoxelGraphEditor::profile() {
	if (_generator.is_null() || !_generator->is_good()) {
		return;
	}

	std::vector<VoxelGeneratorGraph::NodeProfilingInfo> nodes_profiling_info;
	const float us = _generator->debug_measure_microseconds_per_voxel(false, &nodes_profiling_info);
	_profile_label->set_text(String("{0} microseconds per voxel").format(varray(us)));

	struct NodeRatio {
		uint32_t node_id;
		float ratio;
	};

	std::vector<NodeRatio> node_ratios;

	// Deduplicate entries and get maximum
	float max_individual_time = 0.f;
	for (const VoxelGeneratorGraph::NodeProfilingInfo &info : nodes_profiling_info) {
		unsigned int i = 0;
		for (; i < node_ratios.size(); ++i) {
			if (node_ratios[i].node_id == info.node_id) {
				break;
			}
		}
		if (i == node_ratios.size()) {
			node_ratios.push_back(NodeRatio{ info.node_id, float(info.microseconds) });
		} else {
			node_ratios[i].ratio += info.microseconds;
		}
		max_individual_time = math::max(max_individual_time, float(info.microseconds));
	}

	if (max_individual_time > 0.f) {
		for (NodeRatio &nr : node_ratios) {
			nr.ratio = math::clamp(nr.ratio / max_individual_time, 0.f, 1.f);
		}
	} else {
		for (NodeRatio &nr : node_ratios) {
			nr.ratio = 1.f;
		}
	}

	for (const NodeRatio &nr : node_ratios) {
		// Some nodes generated during compilation aren't present in the user-facing graph
		if (!_graph->has_node(nr.node_id)) {
			ZN_PRINT_VERBOSE(format("Ignoring node {} from profiling results, not present in user graph", nr.node_id));
			continue;
		}
		const String ui_node_name = node_to_gui_name(nr.node_id);
		VoxelGraphEditorNode *node_view = get_node_typed<VoxelGraphEditorNode>(*_graph_edit, ui_node_name);
		ERR_CONTINUE(node_view == nullptr);
		node_view->set_profiling_ratio_visible(true);
		node_view->set_profiling_ratio(nr.ratio);
	}
}

static void update_menu_radio_checkable_items(PopupMenu &menu, int checked_id) {
	for (int i = 0; i < menu.get_item_count(); ++i) {
		if (menu.is_item_radio_checkable(i)) {
			const int item_id = menu.get_item_id(i);
			menu.set_item_checked(i, item_id == checked_id);
		}
	}
}

void VoxelGraphEditor::update_preview_axes_menu() {
	// Update menu state from current settings
	ERR_FAIL_COND(_preview_axes_menu == nullptr);
	ToolbarMenuIDs id;
	switch (_preview_axes) {
		case PREVIEW_AXES_XY:
			id = MENU_PREVIEW_AXES_XY;
			break;
		case PREVIEW_AXES_XZ:
			id = MENU_PREVIEW_AXES_XZ;
			break;
		default:
			ERR_PRINT("Unknown preview axes");
			return;
	}
	update_menu_radio_checkable_items(*_preview_axes_menu, id);
}

void VoxelGraphEditor::hide_profiling_ratios() {
	for (int child_index = 0; child_index < _graph_edit->get_child_count(); ++child_index) {
		VoxelGraphEditorNode *node_view = Object::cast_to<VoxelGraphEditorNode>(_graph_edit->get_child(child_index));
		if (node_view == nullptr) {
			continue;
		}
		node_view->set_profiling_ratio_visible(false);
	}
}

void VoxelGraphEditor::update_buttons_availability() {
	// Some features are only available with a generator (for now)
	_debug_menu_button->set_disabled(_generator.is_null());
	_graph_menu_button->set_disabled(_generator.is_null());
}

void VoxelGraphEditor::_on_range_analysis_toggled(bool enabled) {
	schedule_preview_update();
	update_range_analysis_gizmo();
}

void VoxelGraphEditor::_on_range_analysis_area_changed() {
	schedule_preview_update();
	update_range_analysis_gizmo();
}

void VoxelGraphEditor::_on_popout_button_pressed() {
	emit_signal(SIGNAL_POPOUT_REQUESTED);
}

void VoxelGraphEditor::set_popout_button_enabled(bool enable) {
	_popout_button->set_visible(enable);
}

void VoxelGraphEditor::_on_function_file_dialog_file_selected(String fpath) {
	create_function_node(fpath);
}

#ifdef ZN_GODOT

void VoxelGraphEditor::_on_function_quick_open_dialog_quick_open() {
	String path = _function_quick_open_dialog->get_selected();
	if (path == "") {
		return;
	}
	create_function_node(path);
}

#endif

void VoxelGraphEditor::create_function_node(String fpath) {
	Ref<Resource> res = load_resource(fpath);
	if (res.is_null()) {
		ERR_PRINT("Could not instantiate function, resource could not be loaded.");
		return;
	}
	Ref<VoxelGraphFunction> func = res;
	if (func.is_null()) {
		ERR_PRINT(String("Could not instantiate function, resource is not a {0}.")
						  .format(varray(VoxelGraphFunction::get_class_static())));
		return;
	}

	if (func == _graph) {
		ERR_PRINT("Adding a function to itself is not allowed.");
		return;
	}

	if (func->contains_reference_to_function(_graph)) {
		ERR_PRINT("Adding a function to itself is not allowed, detected an indirect cycle.");
		return;
	}

	const Vector2 pos = get_graph_offset_from_mouse(_graph_edit, _click_position);

	const uint32_t node_id = _graph->generate_node_id();
	const StringName node_name = node_to_gui_name(node_id);

	_undo_redo->create_action(ZN_TTR("Create Function Node"));
	_undo_redo->add_do_method(*_graph, "create_function_node", func, pos, node_id);
	_undo_redo->add_do_method(this, "create_node_gui", node_id);
	_undo_redo->add_undo_method(*_graph, "remove_node", node_id);
	_undo_redo->add_undo_method(this, "remove_node_gui", node_name);
	_undo_redo->commit_action();
}

void VoxelGraphEditor::update_functions() {
	struct L {
		static void try_update_node_view(
				VoxelGraphFunction &graph, GraphEdit &graph_edit, uint32_t node_id, const String &node_view_name) {
			if (graph.get_node_type_id(node_id) == VoxelGraphFunction::NODE_FUNCTION) {
				VoxelGraphEditorNode *node_view = get_node_typed<VoxelGraphEditorNode>(graph_edit, node_view_name);
				ERR_FAIL_COND(node_view == nullptr);
				node_view->update_layout(graph);
			}
		}
	};

	ERR_FAIL_COND(_graph.is_null());

	std::vector<ProgramGraph::Connection> removed_connections;
	_graph->update_function_nodes(&removed_connections);
	// TODO This can mess with undo/redo and remove connections, but I'm not sure if it's worth dealing with it.
	// A way to workaround it is to introduce a concept of "invalid ports", where function nodes keep their old ports
	// until they are explicitely removed by an action of the user (and then come back if undone).

	for (const ProgramGraph::Connection &con : removed_connections) {
		const String from_node_name = node_to_gui_name(con.src.node_id);
		const String to_node_name = node_to_gui_name(con.dst.node_id);

		_graph_edit->disconnect_node(from_node_name, con.src.port_index, to_node_name, con.dst.port_index);

		L::try_update_node_view(**_graph, *_graph_edit, con.src.node_id, from_node_name);
		L::try_update_node_view(**_graph, *_graph_edit, con.dst.node_id, to_node_name);
	}
}

void VoxelGraphEditor::_bind_methods() {
#ifdef ZN_GODOT_EXTENSION
	ClassDB::bind_method(D_METHOD("_on_graph_edit_gui_input", "event"), &VoxelGraphEditor::_on_graph_edit_gui_input);
	ClassDB::bind_method(
			D_METHOD("_on_graph_edit_connection_request", "from_node_name", "from_slot", "to_node_name", "to_slot"),
			&VoxelGraphEditor::_on_graph_edit_connection_request);
	ClassDB::bind_method(
			D_METHOD("_on_graph_edit_disconnection_request", "from_node_name", "from_slot", "to_node_name", "to_slot"),
			&VoxelGraphEditor::_on_graph_edit_disconnection_request);
	ClassDB::bind_method(D_METHOD("_on_graph_edit_delete_nodes_request", "node_names"),
			&VoxelGraphEditor::_on_graph_edit_delete_nodes_request);
	ClassDB::bind_method(
			D_METHOD("_on_graph_edit_node_selected", "node"), &VoxelGraphEditor::_on_graph_edit_node_selected);
	ClassDB::bind_method(
			D_METHOD("_on_graph_edit_node_deselected", "node"), &VoxelGraphEditor::_on_graph_edit_node_deselected);
	ClassDB::bind_method(
			D_METHOD("_on_graph_node_dragged", "from", "to", "id"), &VoxelGraphEditor::_on_graph_node_dragged);
	ClassDB::bind_method(D_METHOD("_on_menu_id_pressed", "id"), &VoxelGraphEditor::_on_menu_id_pressed);
	ClassDB::bind_method(D_METHOD("_on_context_menu_id_pressed", "id"), &VoxelGraphEditor::_on_context_menu_id_pressed);
	ClassDB::bind_method(D_METHOD("_on_graph_changed"), &VoxelGraphEditor::_on_graph_changed);
	ClassDB::bind_method(
			D_METHOD("_on_graph_node_name_changed", "node_id"), &VoxelGraphEditor::_on_graph_node_name_changed);
	ClassDB::bind_method(
			D_METHOD("_on_range_analysis_toggled", "enabled"), &VoxelGraphEditor::_on_range_analysis_toggled);
	ClassDB::bind_method(
			D_METHOD("_on_range_analysis_area_changed"), &VoxelGraphEditor::_on_range_analysis_area_changed);
	ClassDB::bind_method(D_METHOD("_on_popout_button_pressed"), &VoxelGraphEditor::_on_popout_button_pressed);
	ClassDB::bind_method(D_METHOD("_on_function_file_dialog_file_selected", "fpath"),
			&VoxelGraphEditor::_on_function_file_dialog_file_selected);
#if ZN_GODOT
	ClassDB::bind_method(D_METHOD("_on_function_quick_open_dialog_quick_open"),
			&VoxelGraphEditor::_on_function_quick_open_dialog_quick_open);
#endif
	ClassDB::bind_method(
			D_METHOD("_on_node_resize_request", "new_size", "node_id"), &VoxelGraphEditor::_on_node_resize_request);
	ClassDB::bind_method(
			D_METHOD("_on_graph_node_preview_gui_input", "event"), &VoxelGraphEditor::_on_graph_node_preview_gui_input);
#endif

	ClassDB::bind_method(D_METHOD("_check_nothing_selected"), &VoxelGraphEditor::_check_nothing_selected);

	ClassDB::bind_method(D_METHOD("create_node_gui", "node_id"), &VoxelGraphEditor::create_node_gui);
	ClassDB::bind_method(D_METHOD("remove_node_gui", "node_name"), &VoxelGraphEditor::remove_node_gui);
	ClassDB::bind_method(D_METHOD("set_node_position", "node_id", "offset"), &VoxelGraphEditor::set_node_position);
	ClassDB::bind_method(D_METHOD("set_node_size", "node_id", "size"), &VoxelGraphEditor::set_node_size);
	ClassDB::bind_method(D_METHOD("update_node_layout", "node_id"), &VoxelGraphEditor::update_node_layout);
	ClassDB::bind_method(D_METHOD("update_node_comment", "node_id"), &VoxelGraphEditor::update_node_comment);

	ADD_SIGNAL(MethodInfo(SIGNAL_NODE_SELECTED, PropertyInfo(Variant::INT, "node_id")));
	ADD_SIGNAL(MethodInfo(SIGNAL_NOTHING_SELECTED));
	ADD_SIGNAL(MethodInfo(SIGNAL_NODES_DELETED));
	ADD_SIGNAL(MethodInfo(SIGNAL_REGENERATE_REQUESTED));
	ADD_SIGNAL(MethodInfo(SIGNAL_POPOUT_REQUESTED));
}

} // namespace zylann::voxel
