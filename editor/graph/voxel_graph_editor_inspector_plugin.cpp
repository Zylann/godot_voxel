#include "voxel_graph_editor_inspector_plugin.h"
#include "editor_property_text_change_on_submit.h"
#include "voxel_graph_node_inspector_wrapper.h"

namespace zylann::voxel {

#if defined(ZN_GODOT)
bool VoxelGraphEditorInspectorPlugin::can_handle(Object *obj) {
#elif defined(ZN_GODOT_EXTENSION)
bool VoxelGraphEditorInspectorPlugin::_can_handle(const Variant &obj_v) const {
	const Object *obj = obj_v;
#endif
	return obj != nullptr && Object::cast_to<VoxelGraphNodeInspectorWrapper>(obj) != nullptr;
}

#if defined(ZN_GODOT)
bool VoxelGraphEditorInspectorPlugin::parse_property(Object *p_object, const Variant::Type p_type, const String &p_path,
		const PropertyHint p_hint, const String &p_hint_text, const uint32_t p_usage, const bool p_wide) {
#elif defined(ZN_GODOT_EXTENSION)
bool VoxelGraphEditorInspectorPlugin::_parse_property(Object *p_object, int64_t p_type, const String &p_path,
		const int64_t p_hint, const String &p_hint_text, const int64_t p_usage, const bool p_wide) {
#endif
	if (p_type == Variant::STRING) {
		add_property_editor(p_path, memnew(ZN_EditorPropertyTextChangeOnSubmit));
		return true;
	}
	return false;
}

} // namespace zylann::voxel
