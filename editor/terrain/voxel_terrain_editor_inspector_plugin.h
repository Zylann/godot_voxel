#ifndef VOXEL_TERRAIN_EDITOR_INSPECTOR_PLUGIN_H
#define VOXEL_TERRAIN_EDITOR_INSPECTOR_PLUGIN_H

#include <editor/editor_inspector.h>

namespace zylann::voxel {

class VoxelTerrainEditorInspectorPlugin : public EditorInspectorPlugin {
	GDCLASS(VoxelTerrainEditorInspectorPlugin, EditorInspectorPlugin)
public:
	bool can_handle(Object *p_object) override;
	bool parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint,
			const String &p_hint_text, const uint32_t p_usage, const bool p_wide = false) override;
};

} // namespace zylann::voxel

#endif // VOXEL_TERRAIN_EDITOR_INSPECTOR_PLUGIN_H
