#include "voxel_instance_library_inspector_plugin.h"
#include "../../constants/voxel_string_names.h"
#include "../../util/godot/classes/button.h"
#include "../../util/godot/classes/h_box_container.h"
#include "../../util/godot/classes/menu_button.h"
#include "../../util/godot/classes/popup_menu.h"
#include "../../util/godot/core/callable.h"
#include "voxel_instance_library_editor_plugin.h"

namespace zylann::voxel {

bool VoxelInstanceLibraryInspectorPlugin::_zn_can_handle(const Object *p_object) const {
	return Object::cast_to<VoxelInstanceLibrary>(p_object) != nullptr;
}

void VoxelInstanceLibraryInspectorPlugin::_zn_parse_begin(Object *p_object) {
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
	const VoxelStringNames &sn = VoxelStringNames::get_singleton();

	// Put buttons on top of the list of items
	HBoxContainer *hb = memnew(HBoxContainer);

	MenuButton *button_add = memnew(MenuButton);
	set_button_icon(*button_add, icon_provider->get_theme_icon(sn.Add, sn.EditorIcons));
	button_add->get_popup()->add_item("MultiMesh item (fast)", BUTTON_ADD_MULTIMESH_ITEM);
	button_add->get_popup()->add_item("Scene item (slow)", BUTTON_ADD_SCENE_ITEM);
	button_add->get_popup()->connect("id_pressed",
			ZN_GODOT_CALLABLE_MP(button_listener, VoxelInstanceLibraryEditorPlugin, _on_add_item_button_pressed));
	hb->add_child(button_add);

	Button *button_remove = memnew(Button);
	set_button_icon(*button_remove, icon_provider->get_theme_icon(sn.Remove, sn.EditorIcons));
	button_remove->set_flat(true);
	button_remove->connect("pressed",
			ZN_GODOT_CALLABLE_MP(button_listener, VoxelInstanceLibraryEditorPlugin, _on_remove_item_button_pressed));
	hb->add_child(button_remove);

	Control *spacer = memnew(Control);
	spacer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	hb->add_child(spacer);

	add_custom_control(hb);
}

} // namespace zylann::voxel
