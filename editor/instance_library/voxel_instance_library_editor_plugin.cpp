#include "voxel_instance_library_editor_plugin.h"
#include "../../terrain/instancing/voxel_instance_library_item.h"
#include "../../terrain/instancing/voxel_instance_library_scene_item.h"

#include <scene/gui/dialogs.h>
#include <scene/gui/menu_button.h>
#include <scene/resources/primitive_meshes.h>

namespace {
enum Buttons {
	BUTTON_ADD_MULTIMESH_ITEM,
	BUTTON_UPDATE_MULTIMESH_ITEM_FROM_SCENE,
	BUTTON_ADD_SCENE_ITEM,
	BUTTON_REMOVE_ITEM
};
} // namespace

bool VoxelInstanceLibraryEditorInspectorPlugin::can_handle(Object *p_object) {
	return Object::cast_to<VoxelInstanceLibrary>(p_object) != nullptr;
}

void VoxelInstanceLibraryEditorInspectorPlugin::parse_begin(Object *p_object) {
	// TODO How can I make sure the buttons will be at the beginning of the "VoxelInstanceLibrary" category?
	// This is a better place than the Spatial editor toolbar (which would get hidden if you are not in the 3D tab
	// of the editor), but it will appear at the very top of the inspector, even above the "VoxelInstanceLibrary"
	// catgeory of properties. That looks a bit off, and if the class were to be inherited, it would start to be
	// confusing because these buttons are about the property list of "VoxelInstanceLibrary" specifically.
	// I could neither use `parse_property` nor `parse_category`, because when the list is empty,
	// the class returns no properties AND no category.
	add_buttons();
}

void VoxelInstanceLibraryEditorInspectorPlugin::add_buttons() {
	CRASH_COND(icon_provider == nullptr);
	CRASH_COND(button_listener == nullptr);

	// Put buttons on top of the list of items
	HBoxContainer *hb = memnew(HBoxContainer);

	MenuButton *button_add = memnew(MenuButton);
	button_add->set_icon(icon_provider->get_icon("Add", "EditorIcons"));
	button_add->get_popup()->add_item("MultiMesh item (fast)", BUTTON_ADD_MULTIMESH_ITEM);
	button_add->get_popup()->add_item("Scene item (slow)", BUTTON_ADD_SCENE_ITEM);
	button_add->get_popup()->connect("id_pressed", button_listener, "_on_button_pressed");
	hb->add_child(button_add);

	Button *button_remove = memnew(Button);
	button_remove->set_icon(icon_provider->get_icon("Remove", "EditorIcons"));
	button_remove->set_flat(true);
	button_remove->connect("pressed", button_listener, "_on_button_pressed", varray(BUTTON_REMOVE_ITEM));
	hb->add_child(button_remove);

	Control *spacer = memnew(Control);
	spacer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	hb->add_child(spacer);

	Button *button_update = memnew(Button);
	button_update->set_text(TTR("Update From Scene..."));
	button_update->connect("pressed", button_listener, "_on_button_pressed",
			varray(BUTTON_UPDATE_MULTIMESH_ITEM_FROM_SCENE));
	hb->add_child(button_update);

	add_custom_control(hb);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

VoxelInstanceLibraryEditorPlugin::VoxelInstanceLibraryEditorPlugin(EditorNode *p_node) {
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
	_open_scene_dialog->set_mode(EditorFileDialog::MODE_OPEN_FILE);
	base_control->add_child(_open_scene_dialog);
	_open_scene_dialog->connect("file_selected", this, "_on_open_scene_dialog_file_selected");
}

bool VoxelInstanceLibraryEditorPlugin::handles(Object *p_object) const {
	VoxelInstanceLibrary *lib = Object::cast_to<VoxelInstanceLibrary>(p_object);
	return lib != nullptr;
}

void VoxelInstanceLibraryEditorPlugin::edit(Object *p_object) {
	VoxelInstanceLibrary *lib = Object::cast_to<VoxelInstanceLibrary>(p_object);
	_library.reference_ptr(lib);
}

void VoxelInstanceLibraryEditorPlugin::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE) {
		Control *base_control = get_editor_interface()->get_base_control();
		_inspector_plugin.instance();
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

void VoxelInstanceLibraryEditorPlugin::_on_button_pressed(int id) {
	_last_used_button = id;

	switch (id) {
		case BUTTON_ADD_MULTIMESH_ITEM: {
			ERR_FAIL_COND(_library.is_null());

			Ref<VoxelInstanceLibraryItem> item;
			item.instance();
			// Setup some defaults
			Ref<CubeMesh> mesh;
			mesh.instance();
			item->set_mesh(mesh, 0);
			item->set_lod_index(2);
			Ref<VoxelInstanceGenerator> generator;
			generator.instance();
			item->set_generator(generator);

			const int item_id = _library->get_next_available_id();

			UndoRedo &ur = get_undo_redo();
			ur.create_action("Add multimesh item");
			ur.add_do_method(*_library, "add_item", item_id, item);
			ur.add_undo_method(*_library, "remove_item", item_id);
			ur.commit_action();
		} break;

		case BUTTON_UPDATE_MULTIMESH_ITEM_FROM_SCENE: {
			ERR_FAIL_COND(_library.is_null());
			const int item_id = try_get_selected_item_id();
			if (item_id != -1) {
				_item_id_to_update = item_id;
				_open_scene_dialog->popup_centered_ratio();
			}
		} break;

		case BUTTON_ADD_SCENE_ITEM: {
			_open_scene_dialog->popup_centered_ratio();
		} break;

		case BUTTON_REMOVE_ITEM: {
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
		_info_dialog->popup_centered();
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
	switch (_last_used_button) {
		case BUTTON_ADD_SCENE_ITEM:
			add_scene_item(fpath);
			break;

		case BUTTON_UPDATE_MULTIMESH_ITEM_FROM_SCENE:
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
	item.instance();
	// Setup some defaults
	item->set_lod_index(2);
	item->set_scene(scene);
	Ref<VoxelInstanceGenerator> generator;
	generator.instance();
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

	Node *node = scene->instance();
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
	ClassDB::bind_method(D_METHOD("_on_button_pressed", "id"), &VoxelInstanceLibraryEditorPlugin::_on_button_pressed);
	ClassDB::bind_method(D_METHOD("_on_remove_item_confirmed"),
			&VoxelInstanceLibraryEditorPlugin::_on_remove_item_confirmed);
	ClassDB::bind_method(D_METHOD("_on_open_scene_dialog_file_selected", "fpath"),
			&VoxelInstanceLibraryEditorPlugin::_on_open_scene_dialog_file_selected);
}
