#ifndef VOXEL_INSTANCE_LIBRARY_INSPECTOR_PLUGIN_H
#define VOXEL_INSTANCE_LIBRARY_INSPECTOR_PLUGIN_H

#include "../../util/godot/classes/editor_inspector_plugin.h"
#include "../../util/macros.h"

ZN_GODOT_FORWARD_DECLARE(class Control)

namespace zylann::voxel {

class VoxelInstanceLibraryEditorPlugin;

class VoxelInstanceLibraryInspectorPlugin : public EditorInspectorPlugin {
	GDCLASS(VoxelInstanceLibraryInspectorPlugin, EditorInspectorPlugin)
public:
	enum Buttons { //
		BUTTON_ADD_MULTIMESH_ITEM,
		BUTTON_ADD_SCENE_ITEM,
		BUTTON_REMOVE_ITEM
	};

	Control *icon_provider = nullptr;
	VoxelInstanceLibraryEditorPlugin *button_listener = nullptr;

#if defined(ZN_GODOT)
	bool can_handle(Object *p_object) override;
	void parse_begin(Object *p_object) override;
#elif defined(ZN_GODOT_EXTENSION)
	bool _can_handle(const Variant &p_object_v) const override;
	void _parse_begin(Object *p_object) override;
#endif

private:
	void add_buttons();

	// When compiling with GodotCpp, `_bind_methods` is not optional.
	static void _bind_methods() {}
};

} // namespace zylann::voxel

#endif // VOXEL_INSTANCE_LIBRARY_INSPECTOR_PLUGIN_H
