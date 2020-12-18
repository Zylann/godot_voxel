#ifndef VOXEL_TERRAIN_EDITOR_PLUGIN_H
#define VOXEL_TERRAIN_EDITOR_PLUGIN_H

#include <editor/editor_plugin.h>

class MenuButton;
class VoxelAboutWindow;
class VoxelNode;

class VoxelTerrainEditorPlugin : public EditorPlugin {
	GDCLASS(VoxelTerrainEditorPlugin, EditorPlugin)
public:
	VoxelTerrainEditorPlugin(EditorNode *p_node);

	bool handles(Object *p_object) const override;
	void edit(Object *p_object) override;
	void make_visible(bool visible) override;

private:
	void set_node(VoxelNode *node);

	void _on_menu_item_selected(int id);
	void _on_terrain_tree_entered(Node *node);
	void _on_terrain_tree_exited(Node *node);

	static void _bind_methods();

	enum MenuID {
		MENU_RESTART_STREAM,
		MENU_REMESH,
		MENU_ABOUT
	};

	VoxelNode *_node = nullptr;

	MenuButton *_menu_button = nullptr;
	VoxelAboutWindow *_about_window = nullptr;
};

#endif // VOXEL_TERRAIN_EDITOR_PLUGIN_H
