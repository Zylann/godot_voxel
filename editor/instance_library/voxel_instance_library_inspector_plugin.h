#ifndef VOXEL_INSTANCE_LIBRARY_INSPECTOR_PLUGIN_H
#define VOXEL_INSTANCE_LIBRARY_INSPECTOR_PLUGIN_H

#include <editor/editor_inspector.h>

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

	bool can_handle(Object *p_object) override;
	void parse_begin(Object *p_object) override;

private:
	void add_buttons();
};

} // namespace zylann::voxel

#endif // VOXEL_INSTANCE_LIBRARY_INSPECTOR_PLUGIN_H
