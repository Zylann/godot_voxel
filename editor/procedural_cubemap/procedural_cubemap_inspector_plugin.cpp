#include "procedural_cubemap_inspector_plugin.h"
#include "../../constants/voxel_string_names.h"
#include "../../generators/graph/procedural_cubemap.h"
#include "../../util/godot/classes/button.h"
#include "../instance_library/control_sizer.h"
#include "procedural_cubemap_viewer.h"

namespace zylann::voxel {

bool VoxelProceduralCubemapInspectorPlugin::_zn_can_handle(const Object *p_object) const {
	return Object::cast_to<VoxelProceduralCubemap>(p_object) != nullptr;
}

void VoxelProceduralCubemapInspectorPlugin::_zn_parse_begin(Object *p_object) {
	Ref<VoxelProceduralCubemap> cubemap = Object::cast_to<VoxelProceduralCubemap>(p_object);

	VoxelProceduralCubemapViewer *viewer = memnew(VoxelProceduralCubemapViewer);
	viewer->set_cubemap(cubemap);
	add_custom_control(viewer);

	ZN_ControlSizer *sizer = memnew(ZN_ControlSizer);
	sizer->set_target_control(viewer);
	add_custom_control(sizer);

	Button *update_button = memnew(Button);
	update_button->set_text("Update");
	update_button->connect(
			"pressed", callable_mp(this, &VoxelProceduralCubemapInspectorPlugin::on_update_button_pressed).bind(cubemap)
	);
	add_custom_control(update_button);

	// TODO Save to texture button
}

void VoxelProceduralCubemapInspectorPlugin::on_update_button_pressed(Ref<VoxelProceduralCubemap> cubemap) {
	ZN_ASSERT_RETURN(cubemap.is_valid());
	cubemap->update();
}

} // namespace zylann::voxel
