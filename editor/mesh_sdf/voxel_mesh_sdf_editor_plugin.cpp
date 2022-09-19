#include "voxel_mesh_sdf_editor_plugin.h"
#include "../../edition/voxel_mesh_sdf_gd.h"
#include "voxel_mesh_sdf_viewer.h"

namespace zylann::voxel {

#if defined(ZN_GODOT)
bool VoxelMeshSDFInspectorPlugin::can_handle(Object *p_object) {
#elif defined(ZN_GODOT_EXTENSION)
bool VoxelMeshSDFInspectorPlugin::_can_handle(const Variant &p_object_v) const {
	const Object *p_object = p_object_v;
#endif
	return Object::cast_to<VoxelMeshSDF>(p_object) != nullptr;
}

#if defined(ZN_GODOT)
void VoxelMeshSDFInspectorPlugin::parse_begin(Object *p_object) {
#elif defined(ZN_GODOT_EXTENSION)
void VoxelMeshSDFInspectorPlugin::_parse_begin(Object *p_object) {
#endif
	VoxelMeshSDFViewer *viewer = memnew(VoxelMeshSDFViewer);
	add_custom_control(viewer);
	VoxelMeshSDF *mesh_sdf = Object::cast_to<VoxelMeshSDF>(p_object);
	ERR_FAIL_COND(mesh_sdf == nullptr);
	viewer->set_mesh_sdf(mesh_sdf);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

VoxelMeshSDFEditorPlugin::VoxelMeshSDFEditorPlugin() {}

#if defined(ZN_GODOT)
bool VoxelMeshSDFEditorPlugin::handles(Object *p_object) const {
#elif defined(ZN_GODOT_EXTENSION)
bool VoxelMeshSDFEditorPlugin::_handles(const Variant &p_object_v) const {
	Object *p_object = p_object_v;
#endif
	ERR_FAIL_COND_V(p_object == nullptr, false);
	return Object::cast_to<VoxelMeshSDF>(p_object) != nullptr;
}

#if defined(ZN_GODOT)
void VoxelMeshSDFEditorPlugin::edit(Object *p_object) {
#elif defined(ZN_GODOT_EXTENSION)
void VoxelMeshSDFEditorPlugin::_edit(const Variant &p_object) {
	//Object *p_object = p_object_v;
#endif
	//_mesh_sdf = p_object;
}

#if defined(ZN_GODOT)
void VoxelMeshSDFEditorPlugin::make_visible(bool visible) {
#elif defined(ZN_GODOT_EXTENSION)
void VoxelMeshSDFEditorPlugin::_make_visible(bool visible) {
#endif
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
