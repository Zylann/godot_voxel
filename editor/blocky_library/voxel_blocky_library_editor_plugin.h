#ifndef VOXEL_BLOCKY_LIBRARY_EDITOR_PLUGIN_H
#define VOXEL_BLOCKY_LIBRARY_EDITOR_PLUGIN_H

#include "../../util/godot/classes/editor_plugin.h"
#include "../../util/godot/macros.h"
#include "../../util/godot/object_weak_ref.h"

ZN_GODOT_FORWARD_DECLARE(class ConfirmationDialog);

namespace zylann::voxel {

class VoxelBlockyTypeLibraryIDSDialog;
class VoxelBlockyTileEditorProperty;
class VoxelBlockyTextureAtlas;
class VoxelBlockyTextureAtlasEditor;

class VoxelBlockyLibraryEditorPlugin : public EditorPlugin {
	GDCLASS(VoxelBlockyLibraryEditorPlugin, EditorPlugin)
public:
	VoxelBlockyLibraryEditorPlugin();

	// This spaghetti is to allow us to re-use the same dialog for each property, otherwise they all get their own
	// dialog, with their own separate size and everything, it's wasteful and annoying
	void open_atlas_tile_picker(Ref<VoxelBlockyTextureAtlas> atlas, VoxelBlockyTileEditorProperty *property);

private:
	void init();

	void on_inspector_atlas_picker_dialog_confirmed();

	void _notification(int p_what);

	// When compiling with GodotCpp, `_bind_methods` is not optional
	static void _bind_methods() {}

	VoxelBlockyTypeLibraryIDSDialog *_type_library_ids_dialog = nullptr;
	VoxelBlockyTextureAtlasEditor *_inspector_atlas_picker = nullptr;
	ConfirmationDialog *_inspector_atlas_picker_dialog = nullptr;
	zylann::godot::ObjectWeakRef<VoxelBlockyTileEditorProperty> _calling_property;
};

} // namespace zylann::voxel

#endif // VOXEL_BLOCKY_LIBRARY_EDITOR_PLUGIN_H
