#ifndef VOXEL_TERRAIN_EDITOR_PLUGIN_H
#define VOXEL_TERRAIN_EDITOR_PLUGIN_H

#include <editor/editor_plugin.h>

class Button;

class VoxelTerrainEditorPlugin : public EditorPlugin {
	GDCLASS(VoxelTerrainEditorPlugin, EditorPlugin)
public:
	VoxelTerrainEditorPlugin(EditorNode *p_node);

	bool handles(Object *p_object) const override;
	void edit(Object *p_object) override;
	void make_visible(bool visible) override;

private:
	void set_node(Node *node);

	void _on_restart_stream_button_pressed();
	void _on_terrain_tree_entered(Node *node);
	void _on_terrain_tree_exited(Node *node);

	static void _bind_methods();

	Button *_restart_stream_button = nullptr;
	Node *_node = nullptr;
};

#endif // VOXEL_TERRAIN_EDITOR_PLUGIN_H
