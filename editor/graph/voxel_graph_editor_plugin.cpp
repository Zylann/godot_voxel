#include "voxel_graph_editor_plugin.h"
#include "../../generators/graph/voxel_generator_graph.h"
#include "../../terrain/voxel_node.h"
#include "../../util/godot/button.h"
#include "../../util/godot/callable.h"
#include "../../util/godot/editor_interface.h"
#include "../../util/godot/editor_scale.h"
#include "../../util/godot/editor_selection.h"
#include "../../util/godot/node.h"
#include "../../util/godot/object.h"
#include "../../util/godot/string.h"
#include "editor_property_text_change_on_submit.h"
#include "voxel_graph_editor.h"
#include "voxel_graph_editor_inspector_plugin.h"
#include "voxel_graph_editor_window.h"
#include "voxel_graph_node_inspector_wrapper.h"

namespace zylann::voxel {

VoxelGraphEditorPlugin::VoxelGraphEditorPlugin() {
	//EditorInterface *ed = get_editor_interface();
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
	Ref<VoxelGraphEditorInspectorPlugin> inspector_plugin;
	inspector_plugin.instantiate();
	add_inspector_plugin(inspector_plugin);
}

#if defined(ZN_GODOT)
bool VoxelGraphEditorPlugin::handles(Object *p_object) const {
#elif defined(ZN_GODOT_EXTENSION)
bool VoxelGraphEditorPlugin::_handles(const Variant &p_object_v) const {
	const Object *p_object = p_object_v;
#endif
	if (p_object == nullptr) {
		return false;
	}
	const VoxelGeneratorGraph *graph_ptr = Object::cast_to<VoxelGeneratorGraph>(p_object);
	if (graph_ptr != nullptr) {
		return true;
	}
	const VoxelGraphNodeInspectorWrapper *wrapper = Object::cast_to<VoxelGraphNodeInspectorWrapper>(p_object);
	if (wrapper != nullptr) {
		return true;
	}
	return false;
}

#if defined(ZN_GODOT)
void VoxelGraphEditorPlugin::edit(Object *p_object) {
#elif defined(ZN_GODOT_EXTENSION)
void VoxelGraphEditorPlugin::_edit(const Variant &p_object_v) {
	Object *p_object = p_object_v;
#endif
	VoxelGeneratorGraph *graph_ptr = Object::cast_to<VoxelGeneratorGraph>(p_object);
	if (graph_ptr == nullptr) {
		VoxelGraphNodeInspectorWrapper *wrapper = Object::cast_to<VoxelGraphNodeInspectorWrapper>(p_object);
		if (wrapper != nullptr) {
			graph_ptr = *wrapper->get_graph();
		}
	}
	Ref<VoxelGeneratorGraph> graph(graph_ptr);
	_graph_editor->set_undo_redo(get_undo_redo()); // UndoRedo isn't available in constructor
	_graph_editor->set_graph(graph);

	VoxelNode *voxel_node = nullptr;
	Array selected_nodes = get_editor_interface()->get_selection()->get_selected_nodes();
	for (int i = 0; i < selected_nodes.size(); ++i) {
		Node *node = Object::cast_to<Node>(selected_nodes[i]);
		ERR_FAIL_COND(node == nullptr);
		VoxelNode *vn = Object::cast_to<VoxelNode>(node);
		if (vn != nullptr && vn->get_generator() == graph_ptr) {
			voxel_node = vn;
			break;
		}
	}
	_voxel_node = voxel_node;
	_graph_editor->set_voxel_node(voxel_node);

	if (_graph_editor_window != nullptr) {
		update_graph_editor_window_title();
	}
}

#if defined(ZN_GODOT)
void VoxelGraphEditorPlugin::make_visible(bool visible) {
#elif defined(ZN_GODOT_EXTENSION)
void VoxelGraphEditorPlugin::_make_visible(bool visible) {
#endif
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
#if defined(ZN_GODOT)
	edit(nullptr);
#elif defined(ZN_GODOT_EXTENSION)
	_edit(nullptr);
#endif
	if (_graph_editor->is_visible_in_tree()) {
		hide_bottom_panel();
	}
}

void VoxelGraphEditorPlugin::_on_graph_editor_node_selected(uint32_t node_id) {
	Ref<VoxelGraphNodeInspectorWrapper> wrapper;
	wrapper.instantiate();
	wrapper->setup(_graph_editor->get_graph(), node_id, get_undo_redo(), _graph_editor);
	// Note: it's neither explicit nor documented, but the reference will stay alive due to EditorHistory::_add_object
	get_editor_interface()->inspect_object(*wrapper);
	// TODO Absurd situation here...
	// `inspect_object()` gets to a point where Godot hides ALL plugins for some reason...
	// And all this, to have the graph editor rebuilt and shown again, because it DOES also handle that resource type
	// -_- https://github.com/godotengine/godot/issues/40166
}

void VoxelGraphEditorPlugin::_on_graph_editor_nothing_selected() {
	// The inspector is a glorious singleton, so when we select nodes to edit their properties, it prevents
	// from accessing the graph resource itself, to save it for example.
	// I'd like to embed properties inside the nodes themselves, but it's a bit more work,
	// so for now I make it so deselecting all nodes in the graph (like clicking in the background) selects the graph.
	Ref<VoxelGeneratorGraph> graph = _graph_editor->get_graph();
	if (graph.is_valid()) {
		get_editor_interface()->inspect_object(*graph);
	}
}

void VoxelGraphEditorPlugin::_on_graph_editor_nodes_deleted() {
	// When deleting nodes, the selected one can be in them, but the inspector wrapper will still point at it.
	// Clean it up and inspect the graph itself.
	Ref<VoxelGeneratorGraph> graph = _graph_editor->get_graph();
	ERR_FAIL_COND(graph.is_null());
	get_editor_interface()->inspect_object(*graph);
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
			Ref<VoxelGeneratorGraph> generator = _graph_editor->get_graph();
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
	queue_free_node(_graph_editor_window);
	_graph_editor_window = nullptr;

	_graph_editor->set_popout_button_enabled(true);

	_bottom_panel_button = add_control_to_bottom_panel(_graph_editor, ZN_TTR("Voxel Graph"));

	_bottom_panel_button->show();
	make_bottom_panel_item_visible(_graph_editor);
}

void VoxelGraphEditorPlugin::update_graph_editor_window_title() {
	ERR_FAIL_COND(_graph_editor_window == nullptr);
	String title;
	if (_graph_editor->get_graph().is_valid()) {
		title = _graph_editor->get_graph()->get_path();
		title += " - ";
	}
	title += VoxelGeneratorGraph::get_class_static();
	_graph_editor_window->set_title(title);
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
