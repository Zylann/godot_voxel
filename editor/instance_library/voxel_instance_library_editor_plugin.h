#ifndef VOXEL_INSTANCE_LIBRARY_EDITOR_PLUGIN_H
#define VOXEL_INSTANCE_LIBRARY_EDITOR_PLUGIN_H

#include "../../terrain/instancing/voxel_instance_library.h"
#include "voxel_instance_library_inspector_plugin.h"
#include <editor/editor_plugin.h>

class Control;
class MenuButton;
class ConfirmationDialog;

namespace zylann::voxel {

class VoxelInstanceLibraryEditorPlugin : public EditorPlugin {
	GDCLASS(VoxelInstanceLibraryEditorPlugin, EditorPlugin)
public:
	virtual String get_name() const override {
		return "VoxelInstanceLibrary";
	}

	VoxelInstanceLibraryEditorPlugin();

	bool handles(Object *p_object) const override;
	void edit(Object *p_object) override;

	void _on_button_pressed(int id);

private:
	void _notification(int p_what);

	int try_get_selected_item_id();
	void add_scene_item(String fpath);

	void _on_remove_item_confirmed();
	void _on_open_scene_dialog_file_selected(String fpath);

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
