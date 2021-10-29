#include "voxel_instance_library_editor_plugin.h"
#include "../../terrain/instancing/voxel_instance_library_item.h"
#include "../../terrain/instancing/voxel_instance_library_scene_item.h"

#include <scene/gui/dialogs.h>
#include <scene/gui/menu_button.h>
#include <scene/resources/primitive_meshes.h>

VoxelInstanceLibraryEditorPlugin::VoxelInstanceLibraryEditorPlugin(EditorNode *p_node) {
	_menu_button = memnew(MenuButton);
	_menu_button->set_text(TTR("VoxelInstanceLibrary"));
	// TODO Icon
	//_menu_button->set_icon(EditorNode::get_singleton()->get_gui_base()->get_icon("MeshLibrary", "EditorIcons"));
	_menu_button->get_popup()->add_item(TTR("Add Multimesh Item (fast)"), MENU_ADD_MULTIMESH_ITEM);
	_menu_button->get_popup()->add_item(TTR("Update Multimesh Item From Scene"), MENU_UPDATE_MULTIMESH_ITEM_FROM_SCENE);
	_menu_button->get_popup()->add_separator();
	_menu_button->get_popup()->add_item(TTR("Add Scene Item (slow)"), MENU_ADD_SCENE_ITEM);
	_menu_button->get_popup()->add_separator();
	_menu_button->get_popup()->add_item(TTR("Remove Selected Item"), MENU_REMOVE_ITEM);
	// TODO Add and update from scene
	_menu_button->get_popup()->connect("id_pressed", this, "_on_menu_id_pressed");
	_menu_button->hide();

	Control *base_control = get_editor_interface()->get_base_control();

	_confirmation_dialog = memnew(ConfirmationDialog);
	_confirmation_dialog->connect("confirmed", this, "_on_remove_item_confirmed");
	base_control->add_child(_confirmation_dialog);

	_info_dialog = memnew(AcceptDialog);
	base_control->add_child(_info_dialog);

	_open_scene_dialog = memnew(EditorFileDialog);
	List<String> extensions;
	ResourceLoader::get_recognized_extensions_for_type("PackedScene", &extensions);
	for (List<String>::Element *E = extensions.front(); E; E = E->next()) {
		_open_scene_dialog->add_filter("*." + E->get());
	}
	// _open_scene_dialog->set_mode(EditorFileDialog::MODE_OPEN_FILE);
	_open_scene_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
	base_control->add_child(_open_scene_dialog);
	_open_scene_dialog->connect("file_selected", this, "_on_open_scene_dialog_file_selected");

	// TODO Perhaps it's a better idea to put this menu in the inspector directly?
	// Because it won't be visible if the user is in 2D or script mode,
	// and it's confusing to see it outside the inspector
	add_control_to_container(EditorPlugin::CONTAINER_SPATIAL_EDITOR_MENU, _menu_button);
}

bool VoxelInstanceLibraryEditorPlugin::handles(Object *p_object) const {
	VoxelInstanceLibrary *lib = Object::cast_to<VoxelInstanceLibrary>(p_object);
	return lib != nullptr;
}

void VoxelInstanceLibraryEditorPlugin::edit(Object *p_object) {
	VoxelInstanceLibrary *lib = Object::cast_to<VoxelInstanceLibrary>(p_object);
	_library.reference_ptr(lib);
}

void VoxelInstanceLibraryEditorPlugin::make_visible(bool visible) {
	_menu_button->set_visible(visible);
}

void VoxelInstanceLibraryEditorPlugin::_on_menu_id_pressed(int id) {
	_last_used_menu_option = MenuOption(id);

	switch (id) {
		case MENU_ADD_MULTIMESH_ITEM: {
			ERR_FAIL_COND(_library.is_null());

			Ref<VoxelInstanceLibraryItem> item;
			item.instantiate();
			// Setup some defaults
			Ref<BoxMesh> mesh;
			mesh.instantiate();
			item->set_mesh(mesh, 0);
			item->set_lod_index(2);
			Ref<VoxelInstanceGenerator> generator;
			generator.instantiate();
			item->set_generator(generator);

			const int item_id = _library->get_next_available_id();

			UndoRedo &ur = get_undo_redo();
			ur.create_action("Add multimesh item");
			ur.add_do_method(*_library, "add_item", item_id, item);
			ur.add_undo_method(*_library, "remove_item", item_id);
			ur.commit_action();
		} break;

		case MENU_UPDATE_MULTIMESH_ITEM_FROM_SCENE: {
			ERR_FAIL_COND(_library.is_null());
			const int item_id = try_get_selected_item_id();
			if (item_id != -1) {
				_item_id_to_update = item_id;
				_open_scene_dialog->popup_centered_ratio();
			}
		} break;

		case MENU_ADD_SCENE_ITEM: {
			_open_scene_dialog->popup_centered_ratio();
		} break;

		case MENU_REMOVE_ITEM: {
			ERR_FAIL_COND(_library.is_null());
			const int item_id = try_get_selected_item_id();
			if (item_id != -1) {
				_item_id_to_remove = item_id;
				_confirmation_dialog->set_text(vformat(TTR("Remove item %d?"), _item_id_to_remove));
				_confirmation_dialog->popup_centered();
			}
		} break;

		default:
			ERR_PRINT("Unknown menu item");
			break;
	}
}

// TODO This function does not modify anything, but cannot be `const` because get_editor_interface() is not...
int VoxelInstanceLibraryEditorPlugin::try_get_selected_item_id() {
	String path = get_editor_interface()->get_inspector()->get_selected_path();
	String prefix = "item_";

	if (path.begins_with(prefix)) {
		const int id = path.substr(prefix.length()).to_int();
		return id;

	} else {
		// The inspector won't let us know which item the current resource is stored into...
		// Gridmap works because it simulates every item as properties of the library,
		// but our current resource does not do that,
		// and I don't want to modify the API just because the built-in inspector is bad.
		_info_dialog->set_text(
				TTR(String("Could not determine selected item from property path: `{0}`.\n"
						   "You must select the `item_X` property label of the item you want to remove."))
						.format(varray(path)));
		_info_dialog->popup();
		return -1;
	}
}

void VoxelInstanceLibraryEditorPlugin::_on_remove_item_confirmed() {
	ERR_FAIL_COND(_library.is_null());
	ERR_FAIL_COND(_item_id_to_remove == -1);

	Ref<VoxelInstanceLibraryItemBase> item = _library->get_item(_item_id_to_remove);

	UndoRedo &ur = get_undo_redo();
	ur.create_action("Remove item");
	ur.add_do_method(*_library, "remove_item", _item_id_to_remove);
	ur.add_undo_method(*_library, "add_item", _item_id_to_remove, item);
	ur.commit_action();

	_item_id_to_remove = -1;
}

void VoxelInstanceLibraryEditorPlugin::_on_open_scene_dialog_file_selected(String fpath) {
	switch (_last_used_menu_option) {
		case MENU_ADD_SCENE_ITEM:
			add_scene_item(fpath);
			break;

		case MENU_UPDATE_MULTIMESH_ITEM_FROM_SCENE:
			update_multimesh_item_from_scene(fpath, _item_id_to_update);
			break;

		default:
			ERR_PRINT("Invalid menu option");
			break;
	}
}

void VoxelInstanceLibraryEditorPlugin::add_scene_item(String fpath) {
	ERR_FAIL_COND(_library.is_null());

	Ref<PackedScene> scene = ResourceLoader::load(fpath);
	ERR_FAIL_COND(scene.is_null());

	Ref<VoxelInstanceLibrarySceneItem> item;
	item.instantiate();
	// Setup some defaults
	item->set_lod_index(2);
	item->set_scene(scene);
	Ref<VoxelInstanceGenerator> generator;
	generator.instantiate();
	generator->set_density(0.01f); // Low density for scenes because that's heavier
	item->set_generator(generator);

	const int item_id = _library->get_next_available_id();

	UndoRedo &ur = get_undo_redo();
	ur.create_action("Add scene item");
	ur.add_do_method(_library.ptr(), "add_item", item_id, item);
	ur.add_undo_method(_library.ptr(), "remove_item", item_id);
	ur.commit_action();
}

void VoxelInstanceLibraryEditorPlugin::update_multimesh_item_from_scene(String fpath, int item_id) {
	ERR_FAIL_COND(_library.is_null());

	Ref<PackedScene> scene = ResourceLoader::load(fpath);
	ERR_FAIL_COND(scene.is_null());

	Ref<VoxelInstanceLibraryItemBase> item_base = _library->get_item(item_id);
	ERR_FAIL_COND_MSG(item_base.is_null(), "Item not found");
	Ref<VoxelInstanceLibraryItem> item = item_base;
	ERR_FAIL_COND_MSG(item.is_null(), "Item not using multimeshes");

	Node *node = scene->instantiate();
	ERR_FAIL_COND(node == nullptr);

	Variant data_before = item->serialize_multimesh_item_properties();
	item->setup_from_template(node);
	Variant data_after = item->serialize_multimesh_item_properties();

	memdelete(node);

	UndoRedo &ur = get_undo_redo();
	ur.create_action("Update Multimesh Item From Scene");
	ur.add_do_method(item.ptr(), "_deserialize_multimesh_item_properties", data_after);
	ur.add_undo_method(item.ptr(), "_deserialize_multimesh_item_properties", data_before);
	ur.commit_action();
}

void VoxelInstanceLibraryEditorPlugin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_on_menu_id_pressed", "id"), &VoxelInstanceLibraryEditorPlugin::_on_menu_id_pressed);
	ClassDB::bind_method(D_METHOD("_on_remove_item_confirmed"),
			&VoxelInstanceLibraryEditorPlugin::_on_remove_item_confirmed);
	ClassDB::bind_method(D_METHOD("_on_open_scene_dialog_file_selected", "fpath"),
			&VoxelInstanceLibraryEditorPlugin::_on_open_scene_dialog_file_selected);
}
