#ifndef VOXEL_INSTANCE_LIBRARY_EDITOR_PLUGIN_H
#define VOXEL_INSTANCE_LIBRARY_EDITOR_PLUGIN_H

#include "../../terrain/instancing/voxel_instance_library.h"
#include "../../util/godot/classes/editor_plugin.h"
#include "voxel_instance_library_inspector_plugin.h"

ZN_GODOT_FORWARD_DECLARE(class Control)
ZN_GODOT_FORWARD_DECLARE(class MenuButton)
ZN_GODOT_FORWARD_DECLARE(class ConfirmationDialog)
ZN_GODOT_FORWARD_DECLARE(class AcceptDialog)

namespace zylann::voxel {

class VoxelInstanceLibraryEditorPlugin : public zylann::godot::ZN_EditorPlugin {
	GDCLASS(VoxelInstanceLibraryEditorPlugin, zylann::godot::ZN_EditorPlugin)
public:
	VoxelInstanceLibraryEditorPlugin();

	// Because this is protected in the base class when compiling as a module
	EditorUndoRedoManager &get_undo_redo2();

protected:
	bool _zn_handles(const Object *p_object) const override;
	void _zn_edit(Object *p_object) override;

	String _zn_get_plugin_name() const override {
		return "VoxelInstanceLibrary";
	}

private:
	void init();
	void _notification(int p_what);

	static void _bind_methods();

	Ref<VoxelInstanceLibraryInspectorPlugin> _inspector_plugin;
};

} // namespace zylann::voxel

#endif // VOXEL_INSTANCE_LIBRARY_EDITOR_PLUGIN_H
