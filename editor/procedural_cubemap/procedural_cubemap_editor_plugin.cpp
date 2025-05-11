#include "procedural_cubemap_editor_plugin.h"
#include "procedural_cubemap_inspector_plugin.h"

namespace zylann::voxel {

VoxelProceduralCubemapEditorPlugin::VoxelProceduralCubemapEditorPlugin() {
	Ref<VoxelProceduralCubemapInspectorPlugin> plugin;
	plugin.instantiate();
	add_inspector_plugin(plugin);
}

void VoxelProceduralCubemapEditorPlugin::_bind_methods() {}

} // namespace zylann::voxel
