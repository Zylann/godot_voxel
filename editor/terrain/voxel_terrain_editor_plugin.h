#ifndef VOXEL_TERRAIN_EDITOR_PLUGIN_H
#define VOXEL_TERRAIN_EDITOR_PLUGIN_H

#include "voxel_terrain_editor_inspector_plugin.h"
#include <editor/editor_plugin.h>

class MenuButton;

namespace zylann::voxel {

class VoxelAboutWindow;
class VoxelNode;
class VoxelTerrainEditorTaskIndicator;

class VoxelTerrainEditorPlugin : public EditorPlugin {
	GDCLASS(VoxelTerrainEditorPlugin, EditorPlugin)
public:
	VoxelTerrainEditorPlugin();

	bool handles(Object *p_object) const override;
	void edit(Object *p_object) override;
	void make_visible(bool visible) override;
	EditorPlugin::AfterGUIInput forward_spatial_gui_input(Camera3D *p_camera, const Ref<InputEvent> &p_event) override;

protected:
	void _notification(int p_what);

private:
	void set_node(VoxelNode *node);
	void generate_menu_items(MenuButton *menu_button, bool is_lod_terrain);

	void _on_menu_item_selected(int id);
	void _on_terrain_tree_entered(Node *node);
	void _on_terrain_tree_exited(Node *node);

	static void _bind_methods();

	enum MenuID { //
		MENU_RESTART_STREAM,
		MENU_REMESH,
		MENU_STREAM_FOLLOW_CAMERA,
		MENU_SHOW_OCTREE_BOUNDS,
		MENU_SHOW_OCTREE_NODES,
		MENU_SHOW_MESH_UPDATES,
		MENU_ABOUT
	};

	VoxelNode *_node = nullptr;

	uint32_t _editor_viewer_id = -1;
	Vector3 _editor_camera_last_position;
	bool _editor_viewer_follows_camera = false;
	bool _show_octree_nodes = false;
	bool _show_octree_bounds = false;
	bool _show_mesh_updates = false;

	MenuButton *_menu_button = nullptr;
	VoxelAboutWindow *_about_window = nullptr;
	VoxelTerrainEditorTaskIndicator *_task_indicator = nullptr;
	Ref<VoxelTerrainEditorInspectorPlugin> _inspector_plugin;
};

} // namespace zylann::voxel

#endif // VOXEL_TERRAIN_EDITOR_PLUGIN_H
