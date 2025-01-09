#ifndef VOXEL_BLOCKY_TILE_SELECTION_INSPECTOR_PLUGIN_H
#define VOXEL_BLOCKY_TILE_SELECTION_INSPECTOR_PLUGIN_H

#include "../../util/godot/classes/editor_inspector_plugin.h"

namespace zylann::voxel {

class VoxelBlockyLibraryEditorPlugin;

class VoxelBlockyTileSelectionInspectorPlugin : public zylann::godot::ZN_EditorInspectorPlugin {
	GDCLASS(VoxelBlockyTileSelectionInspectorPlugin, zylann::godot::ZN_EditorInspectorPlugin)
public:
	void set_plugin(VoxelBlockyLibraryEditorPlugin *plugin);

protected:
	bool _zn_can_handle(const Object *p_object) const override;

	bool _zn_parse_property(
			Object *p_object,
			const Variant::Type p_type,
			const String &p_path,
			const PropertyHint p_hint,
			const String &p_hint_text,
			const BitField<PropertyUsageFlags> p_usage,
			const bool p_wide
	) override;

private:
	static void _bind_methods() {}

	VoxelBlockyLibraryEditorPlugin *_plugin = nullptr;
};

} // namespace zylann::voxel

#endif // VOXEL_BLOCKY_TILE_SELECTION_INSPECTOR_PLUGIN_H
