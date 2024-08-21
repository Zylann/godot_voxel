#ifndef VOXEL_INSTANCE_LIBRARY_LIST_EDITOR_H
#define VOXEL_INSTANCE_LIBRARY_LIST_EDITOR_H

#include "../../terrain/instancing/voxel_instance_library.h"
#include "../../util/godot/classes/h_box_container.h"

ZN_GODOT_FORWARD_DECLARE(class ItemList)
ZN_GODOT_FORWARD_DECLARE(class ConfirmationDialog)
ZN_GODOT_FORWARD_DECLARE(class EditorFileDialog)

namespace zylann::voxel {

class VoxelInstanceLibraryEditorPlugin;

class VoxelInstanceLibraryListEditor : public HBoxContainer {
	GDCLASS(VoxelInstanceLibraryListEditor, HBoxContainer)
public:
	VoxelInstanceLibraryListEditor();

	void setup(const Control *icon_provider, VoxelInstanceLibraryEditorPlugin *plugin);

	void set_library(Ref<VoxelInstanceLibrary> library);

private:
	enum ButtonID { //
		BUTTON_ADD_MULTIMESH_ITEM,
		BUTTON_ADD_SCENE_ITEM,
		BUTTON_REMOVE_ITEM
	};

	void _notification(int p_what);

	void on_list_item_selected(int index);
	void on_button_pressed(int index);
	void on_remove_item_button_pressed();
	void on_remove_item_confirmed();
	void on_open_scene_dialog_file_selected(String fpath);
	// void on_library_item_changed(int id, ChangeType change) override;

	void add_multimesh_item();
	void add_scene_item(String fpath);
	void update_list_from_library();

	static void _bind_methods();

	Ref<VoxelInstanceLibrary> _library;
	ItemList *_item_list = nullptr;
	StdVector<String> _name_cache;

	ButtonID _last_used_button;
	int _item_id_to_remove = -1;

	ConfirmationDialog *_confirmation_dialog = nullptr;
	EditorFileDialog *_open_scene_dialog = nullptr;

	VoxelInstanceLibraryEditorPlugin *_plugin = nullptr;
};

} // namespace zylann::voxel

#endif // VOXEL_INSTANCE_LIBRARY_LIST_EDITOR_H
