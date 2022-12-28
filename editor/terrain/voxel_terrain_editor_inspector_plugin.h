#ifndef VOXEL_TERRAIN_EDITOR_INSPECTOR_PLUGIN_H
#define VOXEL_TERRAIN_EDITOR_INSPECTOR_PLUGIN_H

#include "../../util/godot/classes/editor_inspector_plugin.h"
#include "../../util/macros.h"

namespace zylann::voxel {

class VoxelTerrainEditorInspectorPlugin : public EditorInspectorPlugin {
	GDCLASS(VoxelTerrainEditorInspectorPlugin, EditorInspectorPlugin)
public:
	// In the Godot API, virtual methods all take an `_` by convention, making their name different from modules.
	// TODO GDX: In Godot 4, some virtual methods that took an `Object*` now takes a `Variant`.
	// This looks like a regression from Godot 3.
	// TODO GDX: The Godot API doesn't seem capable of typing enum arguments coming from other classes. Instead they
	// are typed as `int`, making signatures needlessly different and lacking information.
#if defined ZN_GODOT
	bool can_handle(Object *p_object) override;
	bool parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint,
			const String &p_hint_text, const uint32_t p_usage, const bool p_wide = false) override;
#elif defined(ZN_GODOT_EXTENSION)
	bool _can_handle(const Variant &p_object_v) const override;
	bool _parse_property(Object *p_object, int64_t p_type, const String &p_path, int64_t p_hint,
			const String &p_hint_text, int64_t p_usage, const bool p_wide) override;
#endif

	// In GodotCpp, `_bind_methods` is not optional.
#ifdef ZN_GODOT_EXTENSION
private:
	static void _bind_methods() {}
#endif
};

} // namespace zylann::voxel

#endif // VOXEL_TERRAIN_EDITOR_INSPECTOR_PLUGIN_H
