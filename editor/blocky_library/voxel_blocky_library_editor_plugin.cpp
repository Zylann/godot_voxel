#include "voxel_blocky_library_editor_plugin.h"
#include "voxel_blocky_model_editor_inspector_plugin.h"
#include "voxel_blocky_type_editor_inspector_plugin.h"
#include "voxel_blocky_type_library_editor_inspector_plugin.h"
#include "voxel_blocky_type_library_ids_dialog.h"

namespace zylann::voxel {

VoxelBlockyLibraryEditorPlugin::VoxelBlockyLibraryEditorPlugin() {
	{
		Ref<VoxelBlockyModelEditorInspectorPlugin> plugin;
		plugin.instantiate();
		add_inspector_plugin(plugin);
	}

	{
		Ref<VoxelBlockyTypeEditorInspectorPlugin> plugin;
		plugin.instantiate();
		add_inspector_plugin(plugin);
	}

	Control *base_control = get_editor_interface()->get_base_control();

	_type_library_ids_dialog = memnew(VoxelBlockyTypeLibraryIDSDialog);
	base_control->add_child(_type_library_ids_dialog);

	{
		Ref<VoxelBlockyTypeLibraryEditorInspectorPlugin> plugin;
		plugin.instantiate();
		plugin->set_ids_dialog(_type_library_ids_dialog);
		add_inspector_plugin(plugin);
	}
}

} // namespace zylann::voxel
