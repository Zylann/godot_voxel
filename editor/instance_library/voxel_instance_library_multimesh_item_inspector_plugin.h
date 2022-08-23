#ifndef VOXEL_INSTANCE_LIBRARY_MULTIMESH_ITEM_INSPECTOR_PLUGIN_H
#define VOXEL_INSTANCE_LIBRARY_MULTIMESH_ITEM_INSPECTOR_PLUGIN_H

#include <editor/editor_inspector.h>

namespace zylann::voxel {

class VoxelInstanceLibraryMultiMeshItemEditorPlugin;

class VoxelInstanceLibraryMultiMeshItemInspectorPlugin : public EditorInspectorPlugin {
	GDCLASS(VoxelInstanceLibraryMultiMeshItemInspectorPlugin, EditorInspectorPlugin)
public:
	VoxelInstanceLibraryMultiMeshItemEditorPlugin *listener = nullptr;

	bool can_handle(Object *p_object) override;
	void parse_group(Object *p_object, const String &p_group) override;
	bool parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint,
			const String &p_hint_text, const uint32_t p_usage, const bool p_wide = false) override;
};

} // namespace zylann::voxel

#endif // VOXEL_INSTANCE_LIBRARY_MULTIMESH_ITEM_INSPECTOR_PLUGIN_H
