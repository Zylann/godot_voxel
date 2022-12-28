#ifndef VOXEL_INSTANCE_LIBRARY_MULTIMESH_ITEM_INSPECTOR_PLUGIN_H
#define VOXEL_INSTANCE_LIBRARY_MULTIMESH_ITEM_INSPECTOR_PLUGIN_H

#include "../../util/godot/classes/editor_inspector_plugin.h"

namespace zylann::voxel {

class VoxelInstanceLibraryMultiMeshItemEditorPlugin;

class VoxelInstanceLibraryMultiMeshItemInspectorPlugin : public EditorInspectorPlugin {
	GDCLASS(VoxelInstanceLibraryMultiMeshItemInspectorPlugin, EditorInspectorPlugin)
public:
	VoxelInstanceLibraryMultiMeshItemEditorPlugin *listener = nullptr;

#if defined(ZN_GODOT)
	bool can_handle(Object *p_object) override;
	void parse_group(Object *p_object, const String &p_group) override;
	bool parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint,
			const String &p_hint_text, const uint32_t p_usage, const bool p_wide = false) override;
#elif defined(ZN_GODOT_EXTENSION)
	bool _can_handle(const Variant &p_object_v) const override;
	void _parse_group(Object *p_object, const String &p_group) override;
	bool _parse_property(Object *p_object, int64_t p_type, const String &p_path, int64_t p_hint,
			const String &p_hint_text, int64_t p_usage, const bool p_wide) override;
#endif

private:
	// When compiling with GodotCpp, `_bind_methods` is not optional.
	static void _bind_methods() {}
};

} // namespace zylann::voxel

#endif // VOXEL_INSTANCE_LIBRARY_MULTIMESH_ITEM_INSPECTOR_PLUGIN_H
