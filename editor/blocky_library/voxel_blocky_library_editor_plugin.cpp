#include "voxel_blocky_library_editor_plugin.h"
#include "voxel_blocky_model_editor_inspector_plugin.h"

namespace zylann::voxel {

VoxelBlockyLibraryEditorPlugin::VoxelBlockyLibraryEditorPlugin() {
	Ref<VoxelBlockyModelEditorInspectorPlugin> plugin;
	plugin.instantiate();
	add_inspector_plugin(plugin);
}

} // namespace zylann::voxel
