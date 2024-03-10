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

class VoxelTerrainEditorPlugin : public zylann::godot::ZN_EditorPlugin {
	GDCLASS(VoxelTerrainEditorPlugin, zylann::godot::ZN_EditorPlugin)
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

		MENU_DUMP_AS_SCENE,
		MENU_ABOUT,

		MENU_COUNT
	};

	zylann::godot::ObjectWeakRef<VoxelNode> _terrain_node;

	ViewerID _editor_viewer_id;
	bool _editor_viewer_enabled = true;
	Vector3 _editor_camera_last_position;
	bool _editor_viewer_follows_camera = false;

	MenuButton *_menu_button = nullptr;
	VoxelAboutWindow *_about_window = nullptr;
	VoxelTerrainEditorTaskIndicator *_task_indicator = nullptr;
	Ref<VoxelTerrainEditorInspectorPlugin> _inspector_plugin;
	EditorFileDialog *_save_file_dialog = nullptr;
};

} // namespace zylann::voxel

#endif // VOXEL_TERRAIN_EDITOR_PLUGIN_H
