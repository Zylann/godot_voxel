#ifndef VOXEL_TERRAIN_EDITOR_INSPECTOR_PLUGIN_H
#define VOXEL_TERRAIN_EDITOR_INSPECTOR_PLUGIN_H

#include "../../util/godot/classes/editor_inspector_plugin.h"

namespace zylann::voxel {

class VoxelTerrainEditorInspectorPlugin : public zylann::godot::ZN_EditorInspectorPlugin {
	GDCLASS(VoxelTerrainEditorInspectorPlugin, zylann::godot::ZN_EditorInspectorPlugin)
protected:
	bool _zn_can_handle(const Object *p_object) const override;
	bool _zn_parse_property(Object *p_object, const Variant::Type p_type, const String &p_path,
			const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage,
			const bool p_wide = false) override;

private:
	// When compiling with GodotCpp, `_bind_methods` is not optional.
	static void _bind_methods() {}
};

} // namespace zylann::voxel

#endif // VOXEL_TERRAIN_EDITOR_INSPECTOR_PLUGIN_H
