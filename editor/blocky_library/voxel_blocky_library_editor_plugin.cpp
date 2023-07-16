#include "voxel_blocky_library_editor_plugin.h"
#include "../../util/godot/classes/control.h"
#include "../../util/godot/classes/editor_interface.h"
#include "types/voxel_blocky_type_editor_inspector_plugin.h"
#include "types/voxel_blocky_type_library_editor_inspector_plugin.h"
#include "types/voxel_blocky_type_library_ids_dialog.h"
#include "voxel_blocky_model_editor_inspector_plugin.h"

namespace zylann::voxel {

VoxelBlockyLibraryEditorPlugin::VoxelBlockyLibraryEditorPlugin() {}

// TODO GDX: Can't initialize EditorPlugins in their constructor when they access EditorNode.
// See https://github.com/godotengine/godot-cpp/issues/1179
void VoxelBlockyLibraryEditorPlugin::init() {
	EditorUndoRedoManager *undo_redo = get_undo_redo();
	EditorInterface *editor_interface = get_editor_interface();

	// Models
	{
		Ref<VoxelBlockyModelEditorInspectorPlugin> plugin;
		plugin.instantiate();
		plugin->set_undo_redo(undo_redo);
		add_inspector_plugin(plugin);
	}

	// Types
	{
		Ref<VoxelBlockyTypeEditorInspectorPlugin> plugin;
		plugin.instantiate();
		plugin->set_editor_interface(editor_interface);
		plugin->set_undo_redo(undo_redo);
		add_inspector_plugin(plugin);
	}

	Control *base_control = editor_interface->get_base_control();

	_type_library_ids_dialog = memnew(VoxelBlockyTypeLibraryIDSDialog);
	base_control->add_child(_type_library_ids_dialog);

	{
		Ref<VoxelBlockyTypeLibraryEditorInspectorPlugin> plugin;
		plugin.instantiate();
		plugin->set_ids_dialog(_type_library_ids_dialog);
		add_inspector_plugin(plugin);
	}
}

void VoxelBlockyLibraryEditorPlugin::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE) {
		init();
	}
}

} // namespace zylann::voxel
