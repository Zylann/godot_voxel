#ifndef VOXEL_GRAPH_EDITOR_PLUGIN_H
#define VOXEL_GRAPH_EDITOR_PLUGIN_H

#include "../../generators/graph/voxel_graph_function.h"
#include "../../util/godot/classes/editor_plugin.h"
#include "../../util/macros.h"
#include "voxel_graph_node_inspector_wrapper.h"

ZN_GODOT_FORWARD_DECLARE(class Button)

namespace zylann::voxel {

class VoxelGraphEditor;
class VoxelNode;
class VoxelGraphEditorWindow;
class VoxelGraphEditorIODialog;

class VoxelGraphEditorPlugin : public ZN_EditorPlugin {
	GDCLASS(VoxelGraphEditorPlugin, ZN_EditorPlugin)
public:
	VoxelGraphEditorPlugin();

	void edit_ios(Ref<pg::VoxelGraphFunction> graph);

private:
	bool _zn_handles(const Object *p_object) const override;
	void _zn_edit(Object *p_object) override;
	void _zn_make_visible(bool visible) override;

	void _notification(int p_what);

	void undock_graph_editor();
	void dock_graph_editor();
	void update_graph_editor_window_title();
	void inspect_graph_or_generator(const VoxelGraphEditor &graph_editor);

	void _on_graph_editor_node_selected(uint32_t node_id);
	void _on_graph_editor_nothing_selected();
	void _on_graph_editor_nodes_deleted();
	void _on_graph_editor_regenerate_requested();
	void _on_graph_editor_popout_requested();
	void _on_graph_editor_window_close_requested();
	void _hide_deferred();

	static void _bind_methods();

	VoxelGraphEditor *_graph_editor = nullptr;
	VoxelGraphEditorWindow *_graph_editor_window = nullptr;
	VoxelGraphEditorIODialog *_io_dialog = nullptr;
	Button *_bottom_panel_button = nullptr;
	bool _deferred_visibility_scheduled = false;
	VoxelNode *_voxel_node = nullptr;
	std::vector<Ref<VoxelGraphNodeInspectorWrapper>> _node_wrappers;
	// Workaround for a new Godot 4 behavior:
	// When we inspect an object, Godot calls `edit(nullptr)` on our plugin first.
	// But this plugin handles both the graph resource, and nodes in it. When a node is selected, it tells Godot to
	// inspect an associated object, so the inspector can be used to edit properties of the node.
	// But with the new `edit(nullptr)` behavior, the plugin cleans up its UI, which destroys the UI GraphNode you
	// selected, leading to nasty crashes and errors...
	// Since this boils down to the plugin triggering a change in inspected object, we set a boolean to IGNORE
	// `edit(nullptr)` calls.
	bool _ignore_edit_null = false;
};

} // namespace zylann::voxel

#endif // VOXEL_GRAPH_EDITOR_PLUGIN_H
