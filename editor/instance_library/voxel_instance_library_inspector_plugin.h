#ifndef VOXEL_INSTANCE_LIBRARY_INSPECTOR_PLUGIN_H
#define VOXEL_INSTANCE_LIBRARY_INSPECTOR_PLUGIN_H

#include "../../util/godot/classes/editor_inspector_plugin.h"
#include "../../util/godot/macros.h"

ZN_GODOT_FORWARD_DECLARE(class Control)

namespace zylann::voxel {

class VoxelInstanceLibraryEditorPlugin;

class VoxelInstanceLibraryInspectorPlugin : public zylann::godot::ZN_EditorInspectorPlugin {
	GDCLASS(VoxelInstanceLibraryInspectorPlugin, zylann::godot::ZN_EditorInspectorPlugin)
public:
	Control *icon_provider = nullptr;
	VoxelInstanceLibraryEditorPlugin *plugin = nullptr;

protected:
	bool _zn_can_handle(const Object *p_object) const override;
	void _zn_parse_begin(Object *p_object) override;

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
	// When compiling with GodotCpp, `_bind_methods` is not optional.
	static void _bind_methods() {}
};

} // namespace zylann::voxel

#endif // VOXEL_INSTANCE_LIBRARY_INSPECTOR_PLUGIN_H
