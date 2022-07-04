#ifndef VOXEL_MESH_SDF_EDITOR_PLUGIN_H
#define VOXEL_MESH_SDF_EDITOR_PLUGIN_H

#include <editor/editor_plugin.h>

namespace zylann::voxel {

class VoxelMeshSDFInspectorPlugin : public EditorInspectorPlugin {
	GDCLASS(VoxelMeshSDFInspectorPlugin, EditorInspectorPlugin)
public:
	bool can_handle(Object *p_object) override;
	void parse_begin(Object *p_object) override;
};

class VoxelMeshSDFEditorPlugin : public EditorPlugin {
	GDCLASS(VoxelMeshSDFEditorPlugin, EditorPlugin)
public:
	VoxelMeshSDFEditorPlugin();

	bool handles(Object *p_object) const override;
	void edit(Object *p_object) override;
	void make_visible(bool visible) override;

private:
	void _notification(int p_what);

	Ref<VoxelMeshSDFInspectorPlugin> _inspector_plugin;
};

} // namespace zylann::voxel

#endif // VOXEL_MESH_SDF_EDITOR_PLUGIN_H
