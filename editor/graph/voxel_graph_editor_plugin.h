#ifndef VOXEL_GRAPH_EDITOR_PLUGIN_H
#define VOXEL_GRAPH_EDITOR_PLUGIN_H

#include <editor/editor_plugin.h>

namespace zylann::voxel {

class VoxelGraphEditor;
class VoxelNode;
class VoxelGraphEditorWindow;

class VoxelGraphEditorPlugin : public EditorPlugin {
	GDCLASS(VoxelGraphEditorPlugin, EditorPlugin)
public:
	VoxelGraphEditorPlugin();

	bool handles(Object *p_object) const override;
	void edit(Object *p_object) override;
	void make_visible(bool visible) override;

private:
	void undock_graph_editor();
	void dock_graph_editor();
	void update_graph_editor_window_title();

	void _on_graph_editor_node_selected(uint32_t node_id);
	void _on_graph_editor_nothing_selected();
	void _on_graph_editor_nodes_deleted();
	void _on_graph_editor_regenerate_requested();
	void _on_graph_editor_popout_requested();
	void _hide_deferred();

	static void _bind_methods();

	VoxelGraphEditor *_graph_editor = nullptr;
	VoxelGraphEditorWindow *_graph_editor_window = nullptr;
	Button *_bottom_panel_button = nullptr;
	bool _deferred_visibility_scheduled = false;
	VoxelNode *_voxel_node = nullptr;
};

} // namespace zylann::voxel

#endif // VOXEL_GRAPH_EDITOR_PLUGIN_H
