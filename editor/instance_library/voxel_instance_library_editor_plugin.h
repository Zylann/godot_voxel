#ifndef VOXEL_INSTANCE_LIBRARY_EDITOR_PLUGIN_H
#define VOXEL_INSTANCE_LIBRARY_EDITOR_PLUGIN_H

#include "../../terrain/instancing/voxel_instance_library.h"
#include "../../util/godot/classes/editor_plugin.h"
#include "voxel_instance_library_inspector_plugin.h"

ZN_GODOT_FORWARD_DECLARE(class Control)
ZN_GODOT_FORWARD_DECLARE(class MenuButton)
ZN_GODOT_FORWARD_DECLARE(class ConfirmationDialog)
ZN_GODOT_FORWARD_DECLARE(class AcceptDialog)
ZN_GODOT_FORWARD_DECLARE(class EditorFileDialog)

namespace zylann::voxel {

class VoxelInstanceLibraryEditorPlugin : public ZN_EditorPlugin {
	GDCLASS(VoxelInstanceLibraryEditorPlugin, ZN_EditorPlugin)
public:
#ifdef ZN_GODOT
	virtual String get_name() const override {
		return "VoxelInstanceLibrary";
	}
#endif

	VoxelInstanceLibraryEditorPlugin();

	void _on_add_item_button_pressed(int id);
	void _on_remove_item_button_pressed();

protected:
	bool _zn_handles(const Object *p_object) const override;
	void _zn_edit(Object *p_object) override;

private:
	void _notification(int p_what);

	int try_get_selected_item_id();
	void add_scene_item(String fpath);

	void _on_remove_item_confirmed();
	void _on_open_scene_dialog_file_selected(String fpath);

	void _on_button_pressed(int id);

	static void _bind_methods();

	ConfirmationDialog *_confirmation_dialog = nullptr;
	AcceptDialog *_info_dialog = nullptr;
	int _item_id_to_remove = -1;
	int _item_id_to_update = -1;
	EditorFileDialog *_open_scene_dialog;
	int _last_used_button;

	Ref<VoxelInstanceLibrary> _library;
	Ref<VoxelInstanceLibraryInspectorPlugin> _inspector_plugin;
};

} // namespace zylann::voxel

#endif // VOXEL_INSTANCE_LIBRARY_EDITOR_PLUGIN_H
