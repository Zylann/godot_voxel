#include "voxel_blocky_type_editor_inspector_plugin.h"
#include "../../meshers/blocky/types/voxel_blocky_type.h"
#include "voxel_blocky_type_viewer.h"

namespace zylann::voxel {

bool VoxelBlockyTypeEditorInspectorPlugin::_zn_can_handle(const Object *p_object) const {
	return Object::cast_to<VoxelBlockyType>(p_object) != nullptr;
}

void VoxelBlockyTypeEditorInspectorPlugin::_zn_parse_begin(Object *p_object) {
	const VoxelBlockyType *type_ptr = Object::cast_to<VoxelBlockyType>(p_object);
	ZN_ASSERT_RETURN(type_ptr != nullptr);

	Ref<VoxelBlockyType> type(type_ptr);

	VoxelBlockyTypeViewer *viewer = memnew(VoxelBlockyTypeViewer);
	viewer->set_type(type);
	add_custom_control(viewer);
	return;
}

} // namespace zylann::voxel
