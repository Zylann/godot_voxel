#ifndef VOXEL_GRAPH_FUNCTION_INSPECTOR_PLUGIN_H
#define VOXEL_GRAPH_FUNCTION_INSPECTOR_PLUGIN_H

#include "../../generators/graph/voxel_graph_function.h"
#include "../../util/godot/classes/editor_inspector_plugin.h"

namespace zylann::voxel {

class VoxelGraphEditorPlugin;

class VoxelGraphFunctionInspectorPlugin : public ZN_EditorInspectorPlugin {
	GDCLASS(VoxelGraphFunctionInspectorPlugin, ZN_EditorInspectorPlugin)
public:
	bool _zn_can_handle(const Object *obj) const override;
	bool _zn_parse_property(Object *p_object, const Variant::Type p_type, const String &p_path,
			const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage,
			const bool p_wide) override;

	void set_listener(VoxelGraphEditorPlugin *plugin);

private:
	void _on_edit_io_button_pressed(Ref<pg::VoxelGraphFunction> graph);

	// When compiling with GodotCpp, `_bind_methods` is not optional
	static void _bind_methods();

	VoxelGraphEditorPlugin *_listener = nullptr;
};

} // namespace zylann::voxel

#endif // VOXEL_GRAPH_FUNCTION_INSPECTOR_PLUGIN_H
