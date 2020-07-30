#ifndef VOXEL_GRAPH_EDITOR_PLUGIN_H
#define VOXEL_GRAPH_EDITOR_PLUGIN_H

#include <editor/editor_plugin.h>

class VoxelGraphEditor;
//class ToolButton;

class VoxelGraphEditorPlugin : public EditorPlugin {
	GDCLASS(VoxelGraphEditorPlugin, EditorPlugin)
public:
	VoxelGraphEditorPlugin(EditorNode *p_node);

	bool handles(Object *p_object) const override;
	void edit(Object *p_object) override;
	void make_visible(bool visible) override;

private:
	void _on_graph_editor_node_selected(uint32_t node_id);
	void _on_graph_editor_nothing_selected();
	void _hide_deferred();

	static void _bind_methods();

	VoxelGraphEditor *_graph_editor = nullptr;
	ToolButton *_bottom_panel_button = nullptr;
	bool _deferred_visibility_scheduled = false;
};

#endif // VOXEL_GRAPH_EDITOR_PLUGIN_H
