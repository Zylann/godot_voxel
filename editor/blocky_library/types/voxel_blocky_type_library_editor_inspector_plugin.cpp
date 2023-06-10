#include "voxel_blocky_type_library_editor_inspector_plugin.h"
#include "../../../util/godot/classes/button.h"
#include "../../../util/godot/core/callable.h"
#include "../../../util/godot/core/string.h"
#include "voxel_blocky_type_library_ids_dialog.h"

namespace zylann::voxel {

bool VoxelBlockyTypeLibraryEditorInspectorPlugin::_zn_can_handle(const Object *p_object) const {
	return Object::cast_to<VoxelBlockyTypeLibrary>(p_object) != nullptr;
}

void VoxelBlockyTypeLibraryEditorInspectorPlugin::_zn_parse_end(Object *p_object) {
	const VoxelBlockyTypeLibrary *library_ptr = Object::cast_to<VoxelBlockyTypeLibrary>(p_object);
	ZN_ASSERT_RETURN(library_ptr != nullptr);
	Ref<VoxelBlockyTypeLibrary> library(library_ptr);

	Button *button = memnew(Button);
	button->set_text(ZN_TTR("Inspect IDs..."));

#if defined(ZN_GODOT)
	button->connect("pressed",
			ZN_GODOT_CALLABLE_MP(this, VoxelBlockyTypeLibraryEditorInspectorPlugin, _on_inspect_ids_button_pressed)
					.bind(library));
#elif defined(ZN_GODOT_EXTENSION)
	// TODO GDX: `Callable::bind()` isn't implemented in GodotCpp
	ZN_PRINT_ERROR("`Callable::bind()` isn't working in GodotCpp! Can't handle inspecting IDMap from inspector.");
#endif

	// TODO I want to add this button at the end OF THE VoxelBlockyTypeLibrary PART OF THE INSPECTOR,
	// NOT AT THE VERY BOTTOM... but how do I do that?
	add_custom_control(button);
}

void VoxelBlockyTypeLibraryEditorInspectorPlugin::set_ids_dialog(VoxelBlockyTypeLibraryIDSDialog *ids_dialog) {
	_ids_dialog = ids_dialog;
}

void VoxelBlockyTypeLibraryEditorInspectorPlugin::_on_inspect_ids_button_pressed(Ref<VoxelBlockyTypeLibrary> library) {
	ZN_ASSERT_RETURN(_ids_dialog != nullptr);
	_ids_dialog->set_library(library);
	_ids_dialog->popup_centered();
}

void VoxelBlockyTypeLibraryEditorInspectorPlugin::_bind_methods() {
#ifdef ZN_GODOT_EXTENSION
	ClassDB::bind_method(D_METHOD("_on_inspect_ids_button_pressed"),
			&VoxelBlockyTypeLibraryEditorInspectorPlugin::_on_inspect_ids_button_pressed);
#endif
}

} // namespace zylann::voxel
