#include "voxel_instance_library_multimesh_item_editor_plugin.h"
#include "../../util/godot/classes/control.h"
#include "../../util/godot/classes/editor_file_dialog.h"
#include "../../util/godot/classes/editor_interface.h"
#include "../../util/godot/classes/editor_undo_redo_manager.h"
#include "../../util/godot/classes/resource_loader.h"
#include "../../util/godot/core/callable.h"

namespace zylann::voxel {

VoxelInstanceLibraryMultiMeshItemEditorPlugin::VoxelInstanceLibraryMultiMeshItemEditorPlugin() {
	Control *base_control = get_editor_interface()->get_base_control();

	_open_scene_dialog = memnew(EditorFileDialog);
	PackedStringArray extensions = get_recognized_extensions_for_type(PackedScene::get_class_static());
	for (int i = 0; i < extensions.size(); ++i) {
		_open_scene_dialog->add_filter("*." + extensions[i]);
	}
	_open_scene_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
	base_control->add_child(_open_scene_dialog);
	_open_scene_dialog->connect("file_selected",
			ZN_GODOT_CALLABLE_MP(
					this, VoxelInstanceLibraryMultiMeshItemEditorPlugin, _on_open_scene_dialog_file_selected));
}

bool VoxelInstanceLibraryMultiMeshItemEditorPlugin::_zn_handles(const Object *p_object) const {
	// TODO Making a plugin handling sub-resources of `VoxelInstanceLibrary` breaks the inspector.
	// There are also some caveats when using multiple sub-inspectors. To keep supporting multiple sub-inspectors open
	// inside a library, we cannot rely on `edit` giving us edited resources.
	// See https://github.com/godotengine/godot/issues/64700
	return false;
	// const VoxelInstanceLibraryMultiMeshItem *item = Object::cast_to<VoxelInstanceLibraryMultiMeshItem>(p_object);
	// return item != nullptr;
}

void VoxelInstanceLibraryMultiMeshItemEditorPlugin::_zn_edit(Object *p_object) {
	// VoxelInstanceLibraryMultiMeshItem *item = Object::cast_to<VoxelInstanceLibraryMultiMeshItem>(p_object);
	// _item.reference_ptr(item);
}

void VoxelInstanceLibraryMultiMeshItemEditorPlugin::_zn_make_visible(bool visible) {
	// if (!visible) {
	// 	_item.unref();
	// }
}

void VoxelInstanceLibraryMultiMeshItemEditorPlugin::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE) {
		_inspector_plugin.instantiate();
		_inspector_plugin->listener = this;
		add_inspector_plugin(_inspector_plugin);

	} else if (p_what == NOTIFICATION_EXIT_TREE) {
		remove_inspector_plugin(_inspector_plugin);
	}
}

#if defined(ZN_GODOT)
void VoxelInstanceLibraryMultiMeshItemEditorPlugin::_on_update_from_scene_button_pressed(
		VoxelInstanceLibraryMultiMeshItem *item) {
#elif defined(ZN_GODOT_EXTENSION)
void VoxelInstanceLibraryMultiMeshItemEditorPlugin::_on_update_from_scene_button_pressed(Object *item_o) {
	VoxelInstanceLibraryMultiMeshItem *item = Object::cast_to<VoxelInstanceLibraryMultiMeshItem>(item_o);
#endif
	_item.reference_ptr(item);
	ERR_FAIL_COND(_item.is_null());
	_open_scene_dialog->popup_centered_ratio();
}

static void update_multimesh_item_from_scene(
		VoxelInstanceLibraryMultiMeshItem &item, String scene_file_path, EditorUndoRedoManager &ur) {
	Ref<PackedScene> scene = load_resource(scene_file_path);
	ERR_FAIL_COND(scene.is_null());

	Node *node = scene->instantiate();
	ERR_FAIL_COND(node == nullptr);

	Variant data_before = item.serialize_multimesh_item_properties();
	item.setup_from_template(node);
	Variant data_after = item.serialize_multimesh_item_properties();

	memdelete(node);

	ur.create_action("Update Multimesh Item From Scene");
	ur.add_do_method(&item, "_deserialize_multimesh_item_properties", data_after);
	ur.add_undo_method(&item, "_deserialize_multimesh_item_properties", data_before);
	ur.commit_action();
}

void VoxelInstanceLibraryMultiMeshItemEditorPlugin::_on_open_scene_dialog_file_selected(String fpath) {
	ERR_FAIL_COND(_item.is_null());
	update_multimesh_item_from_scene(**_item, fpath, *get_undo_redo());
	// We are done with this item
	_item.unref();
}

void VoxelInstanceLibraryMultiMeshItemEditorPlugin::_bind_methods() {
#ifdef ZN_GODOT_EXTENSION
	ClassDB::bind_method(D_METHOD("_on_update_from_scene_button_pressed", "item"),
			&VoxelInstanceLibraryMultiMeshItemEditorPlugin::_on_update_from_scene_button_pressed);
	ClassDB::bind_method(D_METHOD("_on_open_scene_dialog_file_selected", "path"),
			&VoxelInstanceLibraryMultiMeshItemEditorPlugin::_on_open_scene_dialog_file_selected);
#endif
}

} // namespace zylann::voxel
