#ifndef VOXEL_MESH_SDF_EDITOR_PLUGIN_H
#define VOXEL_MESH_SDF_EDITOR_PLUGIN_H

#include "../../util/godot/classes/editor_inspector_plugin.h"
#include "../../util/godot/classes/editor_plugin.h"

namespace zylann::voxel {

class VoxelMeshSDFInspectorPlugin : public ZN_EditorInspectorPlugin {
	GDCLASS(VoxelMeshSDFInspectorPlugin, ZN_EditorInspectorPlugin)
protected:
	bool _zn_can_handle(const Object *p_object) const override;
	void _zn_parse_begin(Object *p_object) override;

private:
	// When compiling with GodotCpp, `_bind_methods` is not optional.
	static void _bind_methods() {}
};

class VoxelMeshSDFEditorPlugin : public ZN_EditorPlugin {
	GDCLASS(VoxelMeshSDFEditorPlugin, ZN_EditorPlugin)
public:
	VoxelMeshSDFEditorPlugin();

protected:
	bool _zn_handles(const Object *p_object) const override;
	void _zn_edit(Object *p_object) override;
	void _zn_make_visible(bool visible) override;

private:
	void _notification(int p_what);

	// When compiling with GodotCpp, `_bind_methods` is not optional.
	static void _bind_methods() {}

	Ref<VoxelMeshSDFInspectorPlugin> _inspector_plugin;
};

} // namespace zylann::voxel

#endif // VOXEL_MESH_SDF_EDITOR_PLUGIN_H
