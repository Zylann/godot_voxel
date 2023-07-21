#ifndef VOXEL_BLOCKY_LIBRARY_EDITOR_PLUGIN_H
#define VOXEL_BLOCKY_LIBRARY_EDITOR_PLUGIN_H

#include "../../util/godot/classes/editor_plugin.h"

namespace zylann::voxel {

class VoxelBlockyTypeLibraryIDSDialog;

class VoxelBlockyLibraryEditorPlugin : public EditorPlugin {
	GDCLASS(VoxelBlockyLibraryEditorPlugin, EditorPlugin)
public:
	VoxelBlockyLibraryEditorPlugin();

private:
	void init();
	void _notification(int p_what);

	// When compiling with GodotCpp, `_bind_methods` is not optional
	static void _bind_methods() {}

	VoxelBlockyTypeLibraryIDSDialog *_type_library_ids_dialog = nullptr;
};

} // namespace zylann::voxel

#endif // VOXEL_BLOCKY_LIBRARY_EDITOR_PLUGIN_H
