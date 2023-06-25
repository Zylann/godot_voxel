#ifndef VOXEL_BLOCKY_MODEL_EDITOR_INSPECTOR_PLUGIN_H
#define VOXEL_BLOCKY_MODEL_EDITOR_INSPECTOR_PLUGIN_H

#include "../../util/godot/classes/editor_inspector_plugin.h"
#include "../../util/godot/macros.h"

ZN_GODOT_FORWARD_DECLARE(class EditorUndoRedoManager);

namespace zylann::voxel {

class VoxelBlockyModelEditorInspectorPlugin : public ZN_EditorInspectorPlugin {
	GDCLASS(VoxelBlockyModelEditorInspectorPlugin, ZN_EditorInspectorPlugin)
public:
	// TODO GDX: `EditorUndoRedoManager` isn't a singleton yet in GDExtension, so it has to be injected
	void set_undo_redo(EditorUndoRedoManager *urm);

protected:
	bool _zn_can_handle(const Object *p_object) const override;
	void _zn_parse_begin(Object *p_object) override;

private:
	// When compiling with GodotCpp, `_bind_methods` isn't optional.
	static void _bind_methods() {}

	EditorUndoRedoManager *_undo_redo = nullptr;
};

} // namespace zylann::voxel

#endif // VOXEL_BLOCKY_MODEL_EDITOR_INSPECTOR_PLUGIN_H
