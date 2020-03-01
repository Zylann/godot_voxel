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
	VoxelGraphEditor *_graph_editor = nullptr;
	ToolButton *_bottom_panel_button = nullptr;
};

#endif // VOXEL_GRAPH_EDITOR_PLUGIN_H
