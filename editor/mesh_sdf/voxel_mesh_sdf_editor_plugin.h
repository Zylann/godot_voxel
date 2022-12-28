#ifndef VOXEL_MESH_SDF_EDITOR_PLUGIN_H
#define VOXEL_MESH_SDF_EDITOR_PLUGIN_H

#include "../../util/godot/classes/editor_inspector_plugin.h"
#include "../../util/godot/classes/editor_plugin.h"

namespace zylann::voxel {

class VoxelMeshSDFInspectorPlugin : public EditorInspectorPlugin {
	GDCLASS(VoxelMeshSDFInspectorPlugin, EditorInspectorPlugin)
public:
#if defined(ZN_GODOT)
	bool can_handle(Object *p_object) override;
	void parse_begin(Object *p_object) override;
#elif defined(ZN_GODOT_EXTENSION)
	bool _can_handle(const Variant &p_object_v) const override;
	void _parse_begin(Object *p_object) override;
#endif

private:
	// When compiling with GodotCpp, `_bind_methods` is not optional.
	static void _bind_methods() {}
};

class VoxelMeshSDFEditorPlugin : public EditorPlugin {
	GDCLASS(VoxelMeshSDFEditorPlugin, EditorPlugin)
public:
	VoxelMeshSDFEditorPlugin();

#if defined(ZN_GODOT)
	bool handles(Object *p_object) const override;
	void edit(Object *p_object) override;
	void make_visible(bool visible) override;
#elif defined(ZN_GODOT_EXTENSION)
	bool _handles(const Variant &p_object_v) const override;
	void _edit(const Variant &p_object_v) override;
	void _make_visible(bool visible) override;
#endif

private:
	void _notification(int p_what);

	// When compiling with GodotCpp, `_bind_methods` is not optional.
	static void _bind_methods() {}

	Ref<VoxelMeshSDFInspectorPlugin> _inspector_plugin;
};

} // namespace zylann::voxel

#endif // VOXEL_MESH_SDF_EDITOR_PLUGIN_H
