#ifndef VOXEL_GRAPH_EDITOR_INSPECTOR_PLUGIN_H
#define VOXEL_GRAPH_EDITOR_INSPECTOR_PLUGIN_H

#include "../../util/godot/classes/editor_inspector_plugin.h"

namespace zylann::voxel {

// Changes string editors of the inspector to call setters only when enter key is pressed, similar to Unreal.
// Because the default behavior of `EditorPropertyText` is to call the setter on every character typed, which is a
// nightmare when editing an Expression node: inputs change constantly as the code is written which has much higher
// chance of messing up existing connections, and creates individual UndoRedo actions as well.
class VoxelGraphEditorInspectorPlugin : public ZN_EditorInspectorPlugin {
	GDCLASS(VoxelGraphEditorInspectorPlugin, ZN_EditorInspectorPlugin)
protected:
	bool _zn_can_handle(const Object *obj) const override;
	bool _zn_parse_property(Object *p_object, const Variant::Type p_type, const String &p_path,
			const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage,
			const bool p_wide = false) override;

private:
	// When compiling with GodotCpp, `_bind_methods` is not optional
	static void _bind_methods() {}
};

} // namespace zylann::voxel

#endif // VOXEL_GRAPH_EDITOR_INSPECTOR_PLUGIN_H
