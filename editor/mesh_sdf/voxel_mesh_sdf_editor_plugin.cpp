#include "voxel_mesh_sdf_editor_plugin.h"
#include "../../edition/voxel_mesh_sdf_gd.h"
#include "voxel_mesh_sdf_viewer.h"

namespace zylann::voxel {

bool VoxelMeshSDFInspectorPlugin::_zn_can_handle(const Object *p_object) const {
	return Object::cast_to<VoxelMeshSDF>(p_object) != nullptr;
}

void VoxelMeshSDFInspectorPlugin::_zn_parse_begin(Object *p_object) {
	VoxelMeshSDFViewer *viewer = memnew(VoxelMeshSDFViewer);
	add_custom_control(viewer);
	VoxelMeshSDF *mesh_sdf = Object::cast_to<VoxelMeshSDF>(p_object);
	ERR_FAIL_COND(mesh_sdf == nullptr);
	viewer->set_mesh_sdf(mesh_sdf);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

VoxelMeshSDFEditorPlugin::VoxelMeshSDFEditorPlugin() {}

bool VoxelMeshSDFEditorPlugin::_zn_handles(const Object *p_object) const {
	ERR_FAIL_COND_V(p_object == nullptr, false);
	return Object::cast_to<VoxelMeshSDF>(p_object) != nullptr;
}

void VoxelMeshSDFEditorPlugin::_zn_edit(Object *p_object) {
	//_mesh_sdf = p_object;
}

void VoxelMeshSDFEditorPlugin::_zn_make_visible(bool visible) {
	//_mesh_sdf.unref();
}

void VoxelMeshSDFEditorPlugin::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE) {
		_inspector_plugin.instantiate();
		// TODO Why can other Godot plugins do this in the constructor??
		// I found I could not put this in the constructor,
		// otherwise `add_inspector_plugin` causes ANOTHER editor plugin to leak on exit... Oo
		add_inspector_plugin(_inspector_plugin);

	} else if (p_what == NOTIFICATION_EXIT_TREE) {
		remove_inspector_plugin(_inspector_plugin);
	}
}

} // namespace zylann::voxel
