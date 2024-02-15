#ifndef VOXEL_TERRAIN_EDITOR_PLUGIN_H
#define VOXEL_TERRAIN_EDITOR_PLUGIN_H

#include "../../engine/ids.h"
#include "../../util/containers/fixed_array.h"
#include "../../util/godot/classes/editor_plugin.h"
#include "../../util/godot/macros.h"
#include "../../util/godot/object_weak_ref.h"
#include "voxel_terrain_editor_inspector_plugin.h"

// When compiling with GodotCpp, it isn't possible to forward-declare these, due to how virtual methods are implemented.
#include "../../util/godot/classes/camera_3d.h"
#include "../../util/godot/classes/input_event.h"

ZN_GODOT_FORWARD_DECLARE(class MenuButton)
ZN_GODOT_FORWARD_DECLARE(class EditorFileDialog)

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
	void init();
	void add_lod_terrain_debug_draw_option(
			PopupMenu *popup, String text, unsigned int menu_id, unsigned int debug_flag_index);

	void set_voxel_node(VoxelNode *node);

	void generate_menu_items(MenuButton *menu_button, bool is_lod_terrain);

	void _on_menu_item_selected(int id);
	void _on_save_file_dialog_file_selected(String fpath);

	static void _bind_methods();

	enum MenuID { //
		MENU_RESTART_STREAM,
		MENU_REMESH,

		MENU_STREAM_FOLLOW_CAMERA,
		MENU_ENABLE_EDITOR_VIEWER,

		MENU_SHOW_OCTREE_BOUNDS,
		MENU_SHOW_OCTREE_NODES,
		MENU_SHOW_MESH_UPDATES,
		MENU_SHOW_MODIFIER_BOUNDS,
		MENU_SHOW_ACTIVE_MESH_BLOCKS,
		MENU_SHOW_VIEWER_CLIPBOXES,
		MENU_SHOW_ACTIVE_VISUAL_AND_COLLISION_BLOCKS,

		MENU_DUMP_AS_SCENE,
		MENU_ABOUT,

		MENU_COUNT
	};

	ObjectWeakRef<VoxelNode> _terrain_node;

	ViewerID _editor_viewer_id;
	bool _editor_viewer_enabled = true;
	Vector3 _editor_camera_last_position;
	bool _editor_viewer_follows_camera = false;

	uint16_t _lod_terrain_debug_draw_flags = 0;
	FixedArray<int8_t, MENU_COUNT> _menu_id_to_lod_terrain_debug_flag_index;

	MenuButton *_menu_button = nullptr;
	VoxelAboutWindow *_about_window = nullptr;
	VoxelTerrainEditorTaskIndicator *_task_indicator = nullptr;
	Ref<VoxelTerrainEditorInspectorPlugin> _inspector_plugin;
	EditorFileDialog *_save_file_dialog = nullptr;
};

} // namespace zylann::voxel

#endif // VOXEL_TERRAIN_EDITOR_PLUGIN_H
