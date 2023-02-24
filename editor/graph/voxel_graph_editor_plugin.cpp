#include "voxel_graph_editor_plugin.h"
#include "../../generators/graph/voxel_generator_graph.h"
#include "../../terrain/voxel_node.h"
#include "../../util/godot/classes/button.h"
#include "../../util/godot/classes/editor_interface.h"
#include "../../util/godot/classes/editor_selection.h"
#include "../../util/godot/classes/node.h"
#include "../../util/godot/classes/object.h"
#include "../../util/godot/core/callable.h"
#include "../../util/godot/core/string.h"
#include "../../util/godot/editor_scale.h"
#include "editor_property_text_change_on_submit.h"
#include "voxel_graph_editor.h"
#include "voxel_graph_editor_inspector_plugin.h"
#include "voxel_graph_editor_io_dialog.h"
#include "voxel_graph_editor_window.h"
#include "voxel_graph_function_inspector_plugin.h"

namespace zylann::voxel {

using namespace pg;

VoxelGraphEditorPlugin::VoxelGraphEditorPlugin() {
	// EditorInterface *ed = get_editor_interface();
	_graph_editor = memnew(VoxelGraphEditor);
	_graph_editor->set_custom_minimum_size(Size2(0, 300) * EDSCALE);
	_graph_editor->connect(VoxelGraphEditor::SIGNAL_NODE_SELECTED,
			ZN_GODOT_CALLABLE_MP(this, VoxelGraphEditorPlugin, _on_graph_editor_node_selected));
	_graph_editor->connect(VoxelGraphEditor::SIGNAL_NOTHING_SELECTED,
			ZN_GODOT_CALLABLE_MP(this, VoxelGraphEditorPlugin, _on_graph_editor_nothing_selected));
	_graph_editor->connect(VoxelGraphEditor::SIGNAL_NODES_DELETED,
			ZN_GODOT_CALLABLE_MP(this, VoxelGraphEditorPlugin, _on_graph_editor_nodes_deleted));
	_graph_editor->connect(VoxelGraphEditor::SIGNAL_REGENERATE_REQUESTED,
			ZN_GODOT_CALLABLE_MP(this, VoxelGraphEditorPlugin, _on_graph_editor_regenerate_requested));
	_graph_editor->connect(VoxelGraphEditor::SIGNAL_POPOUT_REQUESTED,
			ZN_GODOT_CALLABLE_MP(this, VoxelGraphEditorPlugin, _on_graph_editor_popout_requested));
	_bottom_panel_button = add_control_to_bottom_panel(_graph_editor, ZN_TTR("Voxel Graph"));
	_bottom_panel_button->hide();

	// TODO Move this to `_enter_tree` and remove it on `_exit_tree`?
	Ref<VoxelGraphEditorInspectorPlugin> vge_inspector_plugin;
	vge_inspector_plugin.instantiate();
	add_inspector_plugin(vge_inspector_plugin);

	Ref<VoxelGraphFunctionInspectorPlugin> vgf_inspector_plugin;
	vgf_inspector_plugin.instantiate();
	vgf_inspector_plugin->set_listener(this);
	add_inspector_plugin(vgf_inspector_plugin);
}

bool VoxelGraphEditorPlugin::_zn_handles(const Object *p_object) const {
	if (p_object == nullptr) {
		return false;
	}
	const VoxelGeneratorGraph *generator_ptr = Object::cast_to<VoxelGeneratorGraph>(p_object);
	if (generator_ptr != nullptr) {
		return true;
	}
	const VoxelGraphFunction *graph_ptr = Object::cast_to<VoxelGraphFunction>(p_object);
	if (graph_ptr != nullptr) {
		return true;
	}
	const VoxelGraphNodeInspectorWrapper *wrapper = Object::cast_to<VoxelGraphNodeInspectorWrapper>(p_object);
	if (wrapper != nullptr) {
		return true;
	}
	return false;
}

void VoxelGraphEditorPlugin::_zn_edit(Object *p_object) {
	// Workaround...
	if (p_object == nullptr && _ignore_edit_null) {
		return;
	}

	Ref<VoxelGeneratorGraph> generator;
	Ref<VoxelGraphFunction> graph;
	{
		VoxelGeneratorGraph *generator_ptr = Object::cast_to<VoxelGeneratorGraph>(p_object);
		if (generator_ptr != nullptr) {
			generator.reference_ptr(generator_ptr);
		}
	}
	{
		VoxelGraphFunction *graph_ptr = Object::cast_to<VoxelGraphFunction>(p_object);
		if (graph_ptr != nullptr) {
			graph.reference_ptr(graph_ptr);
		}
	}
	{
		VoxelGraphNodeInspectorWrapper *wrapper_ptr = Object::cast_to<VoxelGraphNodeInspectorWrapper>(p_object);
		if (wrapper_ptr != nullptr) {
			generator = wrapper_ptr->get_generator();
			graph = wrapper_ptr->get_graph();
		}
	}

	_graph_editor->set_undo_redo(get_undo_redo()); // UndoRedo isn't available in constructor

	_graph_editor->set_generator(generator);
	if (generator.is_null()) {
		_graph_editor->set_graph(graph);
	}

	{
		VoxelNode *voxel_node = nullptr;
		if (generator.is_valid()) {
			Array selected_nodes = get_editor_interface()->get_selection()->get_selected_nodes();
			for (int i = 0; i < selected_nodes.size(); ++i) {
				Node *node = Object::cast_to<Node>(selected_nodes[i]);
				ERR_FAIL_COND(node == nullptr);
				VoxelNode *vn = Object::cast_to<VoxelNode>(node);
				if (vn != nullptr && vn->get_generator() == generator) {
					voxel_node = vn;
					break;
				}
			}
		}
		_voxel_node = voxel_node;
		_graph_editor->set_voxel_node(voxel_node);
	}

	if (_graph_editor_window != nullptr) {
		update_graph_editor_window_title();
	}
}

void VoxelGraphEditorPlugin::_zn_make_visible(bool visible) {
	if (_graph_editor_window != nullptr) {
		return;
	}

	if (visible) {
		_bottom_panel_button->show();
		make_bottom_panel_item_visible(_graph_editor);

	} else {
		_voxel_node = nullptr;
		_graph_editor->set_voxel_node(nullptr);

		const bool pinned = _graph_editor_window != nullptr || _graph_editor->is_pinned_hint();
		if (!pinned) {
			_bottom_panel_button->hide();

			// TODO Awful hack to handle the nonsense happening in `_on_graph_editor_node_selected`
			if (!_deferred_visibility_scheduled) {
				_deferred_visibility_scheduled = true;
				call_deferred("_hide_deferred");
			}
		}
	}
}

void VoxelGraphEditorPlugin::_hide_deferred() {
	_deferred_visibility_scheduled = false;
	if (_bottom_panel_button->is_visible()) {
		// Still visible actually? Don't hide then
		return;
	}
	// The point is when the plugin's UI closed (for real, not closed and re-opened simultaneously!),
	// it should cleanup its UI to not waste RAM (as it references stuff).
	_zn_edit(nullptr);

	if (_graph_editor->is_visible_in_tree()) {
		hide_bottom_panel();
	}
}

void VoxelGraphEditorPlugin::_on_graph_editor_node_selected(uint32_t node_id) {
	// We have to make a new wrapper every time because it has to target the same node for a given time.
	Ref<VoxelGraphNodeInspectorWrapper> wrapper;
	wrapper.instantiate();
	wrapper->setup(node_id, _graph_editor);
	// Workaround the new behavior that Godot will call `edit(nullptr)` first when editing another object, even if that
	// object is also handled by the plugin. `edit(nullptr)` would cause the UI to be cleaned up when a GraphNode is
	// emitting its `selected` signal, causing destruction of that GraphNode.
	_ignore_edit_null = true;
	// Note: it's neither explicit nor documented, but the reference will stay alive due to EditorHistory::_add_object.
	// Specifying `inspector_only=true` because that's what other plugins do when they can edit "sub-objects"
	get_editor_interface()->inspect_object(*wrapper);
	_ignore_edit_null = false;
	_node_wrappers.push_back(wrapper);
	// TODO Absurd situation here...
	// `inspect_object()` gets to a point where Godot hides ALL plugins for some reason...
	// And all this, to have the graph editor rebuilt and shown again, because it DOES also handle that resource type
	// -_- https://github.com/godotengine/godot/issues/40166
}

void VoxelGraphEditorPlugin::inspect_graph_or_generator(const VoxelGraphEditor &graph_editor) {
	Ref<VoxelGeneratorGraph> generator = graph_editor.get_generator();
	if (generator.is_valid()) {
		_ignore_edit_null = true;
		get_editor_interface()->inspect_object(*generator);
		_ignore_edit_null = false;
		return;
	}
	Ref<VoxelGraphFunction> graph = graph_editor.get_graph();
	if (graph.is_valid()) {
		_ignore_edit_null = true;
		get_editor_interface()->inspect_object(*graph);
		_ignore_edit_null = false;
		return;
	}
}

void VoxelGraphEditorPlugin::_on_graph_editor_nothing_selected() {
	// The inspector is a glorious singleton, so when we select nodes to edit their properties, it prevents
	// from accessing the graph resource itself, to save it for example.
	// I'd like to embed properties inside the nodes themselves, but it's a bit more work (and a waste of space),
	// so for now I make it so deselecting all nodes in the graph (like clicking in the background) selects the graph.
	inspect_graph_or_generator(*_graph_editor);
}

void VoxelGraphEditorPlugin::_on_graph_editor_nodes_deleted() {
	// When deleting nodes, the selected one can be in them, but the inspector wrapper will still point at it.
	// Clean it up and inspect the graph itself.
	inspect_graph_or_generator(*_graph_editor);
}

template <typename F>
void for_each_node(Node *parent, F action) {
	action(parent);
	for (int i = 0; i < parent->get_child_count(); ++i) {
		for_each_node(parent->get_child(i), action);
	}
}

void VoxelGraphEditorPlugin::_on_graph_editor_regenerate_requested() {
	// We could be editing the graph standalone with no terrain loaded
	if (_voxel_node != nullptr) {
		// Re-generate the selected terrain.
		_voxel_node->restart_stream();

	} else {
		// The node is not selected, but it might be in the tree
		Node *root = get_editor_interface()->get_edited_scene_root();

		if (root != nullptr) {
			Ref<VoxelGeneratorGraph> generator = _graph_editor->get_generator();
			ERR_FAIL_COND(generator.is_null());

			for_each_node(root, [&generator](Node *node) {
				VoxelNode *vnode = Object::cast_to<VoxelNode>(node);
				if (vnode != nullptr && vnode->get_generator() == generator) {
					vnode->restart_stream();
				}
			});
		}
	}
}

void VoxelGraphEditorPlugin::_on_graph_editor_popout_requested() {
	undock_graph_editor();
}

void VoxelGraphEditorPlugin::_on_graph_editor_window_close_requested() {
	dock_graph_editor();
}

void VoxelGraphEditorPlugin::_notification(int p_what) {
	if (p_what == NOTIFICATION_EXIT_TREE) {
		for (Ref<VoxelGraphNodeInspectorWrapper> &w : _node_wrappers) {
			ERR_CONTINUE(w.is_null());
			w->detach_from_graph_editor();
		}
	}
}

void VoxelGraphEditorPlugin::undock_graph_editor() {
	ERR_FAIL_COND(_graph_editor_window != nullptr);
	ZN_PRINT_VERBOSE("Undock voxel graph editor");

	remove_control_from_bottom_panel(_graph_editor);
	_bottom_panel_button = nullptr;

	_graph_editor->set_popout_button_enabled(false);
	_graph_editor->set_anchors_preset(Control::PRESET_FULL_RECT);
	// I don't know what hides it but I needed to make it visible again
	_graph_editor->show();

	_graph_editor_window = memnew(VoxelGraphEditorWindow);
	update_graph_editor_window_title();
	_graph_editor_window->add_child(_graph_editor);
	_graph_editor_window->connect("close_requested",
			ZN_GODOT_CALLABLE_MP(this, VoxelGraphEditorPlugin, _on_graph_editor_window_close_requested));

	Node *base_control = get_editor_interface()->get_base_control();
	base_control->add_child(_graph_editor_window);

	_graph_editor_window->popup_centered_ratio(0.6);
}

void VoxelGraphEditorPlugin::dock_graph_editor() {
	ERR_FAIL_COND(_graph_editor_window == nullptr);
	ZN_PRINT_VERBOSE("Dock voxel graph editor");

	_graph_editor->get_parent()->remove_child(_graph_editor);
	_graph_editor_window->queue_free();
	_graph_editor_window = nullptr;

	_graph_editor->set_popout_button_enabled(true);

	_bottom_panel_button = add_control_to_bottom_panel(_graph_editor, ZN_TTR("Voxel Graph"));

	_bottom_panel_button->show();
	make_bottom_panel_item_visible(_graph_editor);
}

void VoxelGraphEditorPlugin::update_graph_editor_window_title() {
	ERR_FAIL_COND(_graph_editor_window == nullptr);

	String res_path;
	String type_name;

	if (_graph_editor->get_generator().is_valid()) {
		res_path = _graph_editor->get_generator()->get_path();
		type_name = VoxelGeneratorGraph::get_class_static();

	} else if (_graph_editor->get_graph().is_valid()) {
		res_path = _graph_editor->get_graph()->get_path();
		type_name = VoxelGraphFunction::get_class_static();
	}

	String title;
	if (!res_path.is_empty()) {
		title = res_path;
		title += " - ";
		title += type_name;
	} else {
		title = "VoxelGraphEditor (no graph opened)";
	}

	_graph_editor_window->set_title(title);
}

void VoxelGraphEditorPlugin::edit_ios(Ref<VoxelGraphFunction> graph) {
	if (_io_dialog == nullptr) {
		_io_dialog = memnew(VoxelGraphEditorIODialog);
		_io_dialog->set_undo_redo(get_undo_redo());
		Control *base_control = get_editor_interface()->get_base_control();
		base_control->add_child(_io_dialog);
	}

	_io_dialog->set_graph(graph);
	_io_dialog->popup_centered();
}

void VoxelGraphEditorPlugin::_bind_methods() {
#ifdef ZN_GODOT_EXTENSION
	ClassDB::bind_method(D_METHOD("_on_graph_editor_node_selected", "node_id"),
			&VoxelGraphEditorPlugin::_on_graph_editor_node_selected);
	ClassDB::bind_method(
			D_METHOD("_on_graph_editor_nothing_selected"), &VoxelGraphEditorPlugin::_on_graph_editor_nothing_selected);
	ClassDB::bind_method(
			D_METHOD("_on_graph_editor_nodes_deleted"), &VoxelGraphEditorPlugin::_on_graph_editor_nodes_deleted);
	ClassDB::bind_method(D_METHOD("_on_graph_editor_regenerate_requested"),
			&VoxelGraphEditorPlugin::_on_graph_editor_regenerate_requested);
	ClassDB::bind_method(
			D_METHOD("_on_graph_editor_popout_requested"), &VoxelGraphEditorPlugin::_on_graph_editor_popout_requested);
	ClassDB::bind_method(D_METHOD("_on_graph_editor_window_close_requested"),
			&VoxelGraphEditorPlugin::_on_graph_editor_window_close_requested);
#endif
	ClassDB::bind_method(D_METHOD("_hide_deferred"), &VoxelGraphEditorPlugin::_hide_deferred);
}

} // namespace zylann::voxel
