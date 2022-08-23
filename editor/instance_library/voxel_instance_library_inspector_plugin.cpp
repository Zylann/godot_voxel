#include "voxel_instance_library_inspector_plugin.h"
#include "voxel_instance_library_editor_plugin.h"
#include <scene/gui/menu_button.h>

namespace zylann::voxel {

bool VoxelInstanceLibraryInspectorPlugin::can_handle(Object *p_object) {
	return Object::cast_to<VoxelInstanceLibrary>(p_object) != nullptr;
}

void VoxelInstanceLibraryInspectorPlugin::parse_begin(Object *p_object) {
	// TODO How can I make sure the buttons will be at the beginning of the "VoxelInstanceLibrary" category?
	// This is a better place than the Spatial editor toolbar (which would get hidden if you are not in the 3D tab
	// of the editor), but it will appear at the very top of the inspector, even above the "VoxelInstanceLibrary"
	// catgeory of properties. That looks a bit off, and if the class were to be inherited, it would start to be
	// confusing because these buttons are about the property list of "VoxelInstanceLibrary" specifically.
	// I could neither use `parse_property` nor `parse_category`, because when the list is empty,
	// the class returns no properties AND no category.
	add_buttons();
}

void VoxelInstanceLibraryInspectorPlugin::add_buttons() {
	CRASH_COND(icon_provider == nullptr);
	CRASH_COND(button_listener == nullptr);

	// Put buttons on top of the list of items
	HBoxContainer *hb = memnew(HBoxContainer);

	MenuButton *button_add = memnew(MenuButton);
	button_add->set_icon(icon_provider->get_theme_icon(SNAME("Add"), SNAME("EditorIcons")));
	button_add->get_popup()->add_item("MultiMesh item (fast)", BUTTON_ADD_MULTIMESH_ITEM);
	button_add->get_popup()->add_item("Scene item (slow)", BUTTON_ADD_SCENE_ITEM);
	button_add->get_popup()->connect(
			"id_pressed", callable_mp(button_listener, &VoxelInstanceLibraryEditorPlugin::_on_button_pressed));
	hb->add_child(button_add);

	Button *button_remove = memnew(Button);
	button_remove->set_icon(icon_provider->get_theme_icon(SNAME("Remove"), SNAME("EditorIcons")));
	button_remove->set_flat(true);
	button_remove->connect("pressed",
			callable_mp(button_listener, &VoxelInstanceLibraryEditorPlugin::_on_button_pressed)
					.bind(BUTTON_REMOVE_ITEM));
	hb->add_child(button_remove);

	Control *spacer = memnew(Control);
	spacer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	hb->add_child(spacer);

	add_custom_control(hb);
}

} // namespace zylann::voxel
