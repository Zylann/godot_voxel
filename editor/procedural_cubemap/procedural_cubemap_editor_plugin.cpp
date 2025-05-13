#include "procedural_cubemap_editor_plugin.h"
#include "../../constants/voxel_string_names.h"
#include "procedural_cubemap_inspector_plugin.h"

namespace zylann::voxel {

VoxelProceduralCubemapEditorPlugin::VoxelProceduralCubemapEditorPlugin() {}

void VoxelProceduralCubemapEditorPlugin::init() {
	Ref<VoxelProceduralCubemapInspectorPlugin> plugin;
	plugin.instantiate();
	add_inspector_plugin(plugin);
}

void VoxelProceduralCubemapEditorPlugin::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE) {
		init();
	}
}

void VoxelProceduralCubemapEditorPlugin::_bind_methods() {}

} // namespace zylann::voxel
