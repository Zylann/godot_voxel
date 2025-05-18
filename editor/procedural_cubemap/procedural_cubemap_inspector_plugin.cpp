#include "procedural_cubemap_inspector_plugin.h"
#include "../../constants/voxel_string_names.h"
#include "../../generators/graph/procedural_cubemap.h"
#include "procedural_cubemap_viewer.h"

namespace zylann::voxel {

bool VoxelProceduralCubemapInspectorPlugin::_zn_can_handle(const Object *p_object) const {
	return Object::cast_to<VoxelProceduralCubemap>(p_object) != nullptr;
}

void VoxelProceduralCubemapInspectorPlugin::_zn_parse_begin(Object *p_object) {
	Ref<VoxelProceduralCubemap> cubemap = Object::cast_to<VoxelProceduralCubemap>(p_object);

	VoxelProceduralCubemapViewer *viewer = memnew(VoxelProceduralCubemapViewer);
	viewer->set_cubemap(cubemap);
	viewer->set_ctrl_to_zoom(true); // To avoid clashing with inspector scrolling
	add_custom_control(viewer);
}

} // namespace zylann::voxel
