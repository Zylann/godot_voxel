#ifndef VOXEL_INSTANCE_LIBRARY_EDITOR_PLUGIN_H
#define VOXEL_INSTANCE_LIBRARY_EDITOR_PLUGIN_H

#include "../../terrain/instancing/voxel_instance_library.h"
#include <editor/editor_plugin.h>

class MenuButton;
class ConfirmationDialog;

class VoxelInstanceLibraryEditorPlugin : public EditorPlugin {
	GDCLASS(VoxelInstanceLibraryEditorPlugin, EditorPlugin)
public:
	virtual String get_name() const { return "VoxelInstanceLibrary"; }

	VoxelInstanceLibraryEditorPlugin(EditorNode *p_node);

	bool handles(Object *p_object) const override;
	void edit(Object *p_object) override;
	void make_visible(bool visible) override;

private:
	int try_get_selected_item_id();
	void add_scene_item(String fpath);
	void update_multimesh_item_from_scene(String fpath, int item_id);

	void _on_menu_id_pressed(int id);
	void _on_remove_item_confirmed();
	void _on_open_scene_dialog_file_selected(String fpath);

	static void _bind_methods();

	enum MenuOption {
		MENU_ADD_MULTIMESH_ITEM,
		MENU_UPDATE_MULTIMESH_ITEM_FROM_SCENE,
		MENU_ADD_SCENE_ITEM,
		MENU_REMOVE_ITEM
	};

	MenuButton *_menu_button = nullptr;
	ConfirmationDialog *_confirmation_dialog = nullptr;
	AcceptDialog *_info_dialog = nullptr;
	int _item_id_to_remove = -1;
	int _item_id_to_update = -1;
	EditorFileDialog *_open_scene_dialog;
	MenuOption _last_used_menu_option;

	Ref<VoxelInstanceLibrary> _library;
};

#endif // VOXEL_INSTANCE_LIBRARY_EDITOR_PLUGIN_H
