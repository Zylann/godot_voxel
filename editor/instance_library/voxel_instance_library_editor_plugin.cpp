#include "voxel_instance_library_editor_plugin.h"
#include "../../terrain/instancing/voxel_instance_library_multimesh_item.h"
#include "../../terrain/instancing/voxel_instance_library_scene_item.h"
#include "../../util/godot/classes/box_mesh.h"
#include "../../util/godot/classes/confirmation_dialog.h"
#include "../../util/godot/classes/control.h"
#include "../../util/godot/classes/editor_file_dialog.h"
#include "../../util/godot/classes/editor_inspector.h"
#include "../../util/godot/classes/editor_interface.h"
#include "../../util/godot/classes/editor_undo_redo_manager.h"
#include "../../util/godot/classes/object.h"
#include "../../util/godot/classes/resource_loader.h"
#include "../../util/godot/core/array.h"
#include "../../util/godot/core/callable.h"
#include "../../util/godot/core/string.h"

namespace zylann::voxel {

VoxelInstanceLibraryEditorPlugin::VoxelInstanceLibraryEditorPlugin() {
	Control *base_control = get_editor_interface()->get_base_control();

	_confirmation_dialog = memnew(ConfirmationDialog);
	_confirmation_dialog->connect(
			"confirmed", ZN_GODOT_CALLABLE_MP(this, VoxelInstanceLibraryEditorPlugin, _on_remove_item_confirmed));
	base_control->add_child(_confirmation_dialog);

	_info_dialog = memnew(AcceptDialog);
	base_control->add_child(_info_dialog);

	_open_scene_dialog = memnew(EditorFileDialog);
	PackedStringArray extensions = get_recognized_extensions_for_type(PackedScene::get_class_static());
	for (int i = 0; i < extensions.size(); ++i) {
		_open_scene_dialog->add_filter("*." + extensions[i]);
	}
	_open_scene_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
	base_control->add_child(_open_scene_dialog);
	_open_scene_dialog->connect("file_selected",
			ZN_GODOT_CALLABLE_MP(this, VoxelInstanceLibraryEditorPlugin, _on_open_scene_dialog_file_selected));
}

bool VoxelInstanceLibraryEditorPlugin::_zn_handles(const Object *p_object) const {
	const VoxelInstanceLibrary *lib = Object::cast_to<VoxelInstanceLibrary>(p_object);
	return lib != nullptr;
}

void VoxelInstanceLibraryEditorPlugin::_zn_edit(Object *p_object) {
	VoxelInstanceLibrary *lib = Object::cast_to<VoxelInstanceLibrary>(p_object);
	_library.reference_ptr(lib);
}

void VoxelInstanceLibraryEditorPlugin::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE) {
		Control *base_control = get_editor_interface()->get_base_control();
		_inspector_plugin.instantiate();
		_inspector_plugin->button_listener = this;
		_inspector_plugin->icon_provider = base_control;
		// TODO Why can other Godot plugins do this in the constructor??
		// I found I could not put this in the constructor,
		// otherwise `add_inspector_plugin` causes ANOTHER editor plugin to leak on exit... Oo
		add_inspector_plugin(_inspector_plugin);

	} else if (p_what == NOTIFICATION_EXIT_TREE) {
		remove_inspector_plugin(_inspector_plugin);
	}
}

void VoxelInstanceLibraryEditorPlugin::_on_add_item_button_pressed(int id) {
	_on_button_pressed(id);
}

void VoxelInstanceLibraryEditorPlugin::_on_remove_item_button_pressed() {
	_on_button_pressed(VoxelInstanceLibraryInspectorPlugin::BUTTON_REMOVE_ITEM);
}

void VoxelInstanceLibraryEditorPlugin::_on_button_pressed(int id) {
	_last_used_button = id;

	switch (id) {
		case VoxelInstanceLibraryInspectorPlugin::BUTTON_ADD_MULTIMESH_ITEM: {
			ERR_FAIL_COND(_library.is_null());

			Ref<VoxelInstanceLibraryMultiMeshItem> item;
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

			EditorUndoRedoManager &ur = *get_undo_redo();
			ur.create_action("Add multimesh item");
			ur.add_do_method(*_library, "add_item", item_id, item);
			ur.add_undo_method(*_library, "remove_item", item_id);
			ur.commit_action();
		} break;

		case VoxelInstanceLibraryInspectorPlugin::BUTTON_ADD_SCENE_ITEM: {
			_open_scene_dialog->popup_centered_ratio();
		} break;

		case VoxelInstanceLibraryInspectorPlugin::BUTTON_REMOVE_ITEM: {
			ERR_FAIL_COND(_library.is_null());
			const int item_id = try_get_selected_item_id();
			if (item_id != -1) {
				_item_id_to_remove = item_id;
				_confirmation_dialog->set_text(ZN_TTR("Remove item {0}?").format(varray(_item_id_to_remove)));
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
				ZN_TTR(String("Could not determine selected item from property path: `{0}`.\n"
							  "You must select the `item_X` property label of the item you want to remove."))
						.format(varray(path)));
		_info_dialog->popup_centered();
		return -1;
	}
}

void VoxelInstanceLibraryEditorPlugin::_on_remove_item_confirmed() {
	ERR_FAIL_COND(_library.is_null());
	ERR_FAIL_COND(_item_id_to_remove == -1);

	Ref<VoxelInstanceLibraryItem> item = _library->get_item(_item_id_to_remove);

	EditorUndoRedoManager &ur = *get_undo_redo();
	ur.create_action("Remove item");
	ur.add_do_method(*_library, "remove_item", _item_id_to_remove);
	ur.add_undo_method(*_library, "add_item", _item_id_to_remove, item);
	ur.commit_action();

	_item_id_to_remove = -1;
}

void VoxelInstanceLibraryEditorPlugin::_on_open_scene_dialog_file_selected(String fpath) {
	switch (_last_used_button) {
		case VoxelInstanceLibraryInspectorPlugin::BUTTON_ADD_SCENE_ITEM:
			add_scene_item(fpath);
			break;

		default:
			ERR_PRINT("Invalid menu option");
			break;
	}
}

void VoxelInstanceLibraryEditorPlugin::add_scene_item(String fpath) {
	ERR_FAIL_COND(_library.is_null());

	Ref<PackedScene> scene = load_resource(fpath);
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

	EditorUndoRedoManager &ur = *get_undo_redo();
	ur.create_action("Add scene item");
	ur.add_do_method(_library.ptr(), "add_item", item_id, item);
	ur.add_undo_method(_library.ptr(), "remove_item", item_id);
	ur.commit_action();
}

void VoxelInstanceLibraryEditorPlugin::_bind_methods() {
#ifdef ZN_GODOT_EXTENSION
	ClassDB::bind_method(D_METHOD("_on_add_multimesh_item_button_pressed"),
			&VoxelInstanceLibraryEditorPlugin::_on_add_item_button_pressed);
	ClassDB::bind_method(D_METHOD("_on_remove_item_button_pressed"),
			&VoxelInstanceLibraryEditorPlugin::_on_remove_item_button_pressed);
	// ClassDB::bind_method(D_METHOD("_on_button_pressed", "id"),
	// &VoxelInstanceLibraryEditorPlugin::_on_button_pressed);
	ClassDB::bind_method(
			D_METHOD("_on_remove_item_confirmed"), &VoxelInstanceLibraryEditorPlugin::_on_remove_item_confirmed);
	ClassDB::bind_method(D_METHOD("_on_open_scene_dialog_file_selected", "fpath"),
			&VoxelInstanceLibraryEditorPlugin::_on_open_scene_dialog_file_selected);
#endif
}

} // namespace zylann::voxel
