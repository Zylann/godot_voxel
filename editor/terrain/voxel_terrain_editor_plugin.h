#ifndef VOXEL_TERRAIN_EDITOR_PLUGIN_H
#define VOXEL_TERRAIN_EDITOR_PLUGIN_H

#include "../../engine/ids.h"
#include "../../util/godot/classes/editor_plugin.h"
#include "voxel_terrain_editor_inspector_plugin.h"

// When compiling with GodotCpp, it isn't possible to forward-declare these, due to how virtual methods are implemented.
#include "../../util/godot/classes/camera_3d.h"
#include "../../util/godot/classes/input_event.h"

ZN_GODOT_FORWARD_DECLARE(class MenuButton)

namespace zylann::voxel {

class VoxelAboutWindow;
class VoxelNode;
class VoxelTerrainEditorTaskIndicator;

class VoxelTerrainEditorPlugin : public ZN_EditorPlugin {
	GDCLASS(VoxelTerrainEditorPlugin, ZN_EditorPlugin)
public:
	VoxelTerrainEditorPlugin();

protected:
	bool _zn_handles(const Object *p_object) const override;
	void _zn_edit(Object *p_object) override;
	void _zn_make_visible(bool visible) override;
	EditorPlugin::AfterGUIInput _zn_forward_3d_gui_input(Camera3D *p_camera, const Ref<InputEvent> &p_event) override;

	void _notification(int p_what);

private:
	void set_node(VoxelNode *node);
	void generate_menu_items(MenuButton *menu_button, bool is_lod_terrain);

	void _on_menu_item_selected(int id);
#if defined(ZN_GODOT)
	void _on_terrain_tree_entered(Node *node);
	void _on_terrain_tree_exited(Node *node);
#elif defined(ZN_GODOT_EXTENSION)
	// TODO GDX: it seems binding a method taking a `Node*` fails to compile. It is supposed to be working.
	// This doubled down with the fact we can't use direct method pointers with `Callable`, hence the need to bind.
	void _on_terrain_tree_entered(Object *node_o);
	void _on_terrain_tree_exited(Object *node_o);
#endif

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

	ViewerID _editor_viewer_id;
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
