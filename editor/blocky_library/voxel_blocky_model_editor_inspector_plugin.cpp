#include "voxel_blocky_model_editor_inspector_plugin.h"
#include "voxel_blocky_model_viewer.h"

namespace zylann::voxel {

bool VoxelBlockyModelEditorInspectorPlugin::_zn_can_handle(const Object *p_object) const {
	return Object::cast_to<VoxelBlockyModel>(p_object) != nullptr;
}

void VoxelBlockyModelEditorInspectorPlugin::_zn_parse_begin(Object *p_object) {
	const VoxelBlockyModel *model_ptr = Object::cast_to<VoxelBlockyModel>(p_object);
	ZN_ASSERT_RETURN(model_ptr != nullptr);

	Ref<VoxelBlockyModel> model(model_ptr);

	VoxelBlockyModelViewer *viewer = memnew(VoxelBlockyModelViewer);
	viewer->set_model(model);
	add_custom_control(viewer);
	return;
}

} // namespace zylann::voxel
