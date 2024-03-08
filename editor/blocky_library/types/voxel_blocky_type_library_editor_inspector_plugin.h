#ifndef VOXEL_BLOCKY_TYPE_LIBRARY_EDITOR_INSPECTOR_PLUGIN_H
#define VOXEL_BLOCKY_TYPE_LIBRARY_EDITOR_INSPECTOR_PLUGIN_H

#include "../../../meshers/blocky/types/voxel_blocky_type_library.h"
#include "../../../util/godot/classes/editor_inspector_plugin.h"

namespace zylann::voxel {

class VoxelBlockyTypeLibraryIDSDialog;

class VoxelBlockyTypeLibraryEditorInspectorPlugin : public zylann::godot::ZN_EditorInspectorPlugin {
	GDCLASS(VoxelBlockyTypeLibraryEditorInspectorPlugin, zylann::godot::ZN_EditorInspectorPlugin)
public:
	void set_ids_dialog(VoxelBlockyTypeLibraryIDSDialog *ids_dialog);

protected:
	bool _zn_can_handle(const Object *p_object) const override;
	void _zn_parse_end(Object *p_object) override;

private:
	void _on_inspect_ids_button_pressed(Ref<VoxelBlockyTypeLibrary> library);

	static void _bind_methods();

	VoxelBlockyTypeLibraryIDSDialog *_ids_dialog = nullptr;
};

} // namespace zylann::voxel

#endif // VOXEL_BLOCKY_TYPE_LIBRARY_EDITOR_INSPECTOR_PLUGIN_H
