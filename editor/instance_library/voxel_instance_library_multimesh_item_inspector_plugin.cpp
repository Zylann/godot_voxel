#include "voxel_instance_library_multimesh_item_inspector_plugin.h"
#include "../../terrain/instancing/voxel_instance_library_multimesh_item.h"
#include "../../util/godot/classes/button.h"
#include "../../util/godot/classes/label.h"
#include "../../util/godot/core/string.h"
#include "voxel_instance_library_editor_plugin.h"
#include "voxel_instance_library_multimesh_item_editor_plugin.h"

namespace zylann::voxel {

#if defined(ZN_GODOT)
bool VoxelInstanceLibraryMultiMeshItemInspectorPlugin::can_handle(Object *p_object) {
#elif defined(ZN_GODOT_EXTENSION)
bool VoxelInstanceLibraryMultiMeshItemInspectorPlugin::_can_handle(const Variant &p_object_v) const {
	Object *p_object = p_object_v;
#endif
	return Object::cast_to<VoxelInstanceLibraryMultiMeshItem>(p_object) != nullptr;
}

#if defined(ZN_GODOT)
void VoxelInstanceLibraryMultiMeshItemInspectorPlugin::parse_group(Object *p_object, const String &p_group) {
#elif defined(ZN_GODOT_EXTENSION)
void VoxelInstanceLibraryMultiMeshItemInspectorPlugin::_parse_group(Object *p_object, const String &p_group) {
#endif
	const VoxelInstanceLibraryMultiMeshItem *item = Object::cast_to<VoxelInstanceLibraryMultiMeshItem>(p_object);
	ERR_FAIL_COND(item == nullptr);
	if (item->get_scene().is_null()) {
		ERR_FAIL_COND(listener == nullptr);
		// TODO I preferred  this at the end of the group, but Godot doesn't expose anything to do it.
		// This is a legacy workflow, we'll see if it can be removed later.
		Button *button = memnew(Button);
		button->set_tooltip_text(
				ZN_TTR("Set properties based on an existing scene. This might copy mesh and material data if "
					   "the scene embeds them. Properties will not update if the scene changes later."));
		button->set_text(ZN_TTR("Update from scene..."));
#if defined(ZN_GODOT)
		// Using a bind() instead of relying on "currently edited" item in the editor plugin allows to support multiple
		// sub-inspectors. Plugins are not instanced per-inspected-object, but custom controls are.
		button->connect("pressed",
				callable_mp(
						listener, &VoxelInstanceLibraryMultiMeshItemEditorPlugin::_on_update_from_scene_button_pressed)
						.bind(item));
#elif defined(ZN_GODOT_EXTENSION)
		// TODO GDX: Need to use Callable::bind() but it has no implementation
		// See https://github.com/godotengine/godot-cpp/issues/802
		ZN_PRINT_ERROR("Unable to setup button to update `VoxelInstanceLibraryMultiMeshItem` from a scene with "
					   "GDExtension! Callable::bind isn't working.");
#endif
		add_custom_control(button);
		return;
	}

	if (p_group == VoxelInstanceLibraryMultiMeshItem::MANUAL_SETTINGS_GROUP_NAME) {
		Label *label = memnew(Label);
		label->set_text(ZN_TTR("Properties are defined by scene."));
		add_custom_control(label);
	}
	// TODO Button to open scene in editor, since Godot doesn't have that in its resource picker menu?
	// Perhaps it should rather be a feature request to Godot.
	// else if (p_group == VoxelInstanceLibraryMultiMeshItem::SCENE_SETTINGS_GROUP_NAME) {
	// 	ERR_FAIL_COND(listener == nullptr);
	// 	Button *button = memnew(Button);
	// 	button->set_text(TTR("Open scene in editor"));
	// }
}

#if defined(ZN_GODOT)
bool VoxelInstanceLibraryMultiMeshItemInspectorPlugin::parse_property(Object *p_object, const Variant::Type p_type,
		const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const uint32_t p_usage,
		const bool p_wide) {
#elif defined(ZN_GODOT_EXTENSION)
bool VoxelInstanceLibraryMultiMeshItemInspectorPlugin::_parse_property(Object *p_object, int64_t p_type,
		const String &p_path, int64_t p_hint, const String &p_hint_text, int64_t p_usage, const bool p_wide) {
#endif
	// Hide manual properties if a scene is assigned, because it will override them
	const VoxelInstanceLibraryMultiMeshItem *item = Object::cast_to<VoxelInstanceLibraryMultiMeshItem>(p_object);
	ERR_FAIL_COND_V(item == nullptr, false);
	if (item->get_scene().is_null()) {
		return false;
	}
	// TODO I only want to make all properties read-only if they are in the "Manual properties" category...
	// But Godot doesn't seem to expose a way to do either, so all I can do is hide them one by one
	static const char *s_manual_properties[] = {
		"cast_shadow", //
		"collision_layer", //
		"collision_mask", //
		"collision_shapes", //
		"material_override", //
		"mesh", //
		"mesh_lod1", //
		"mesh_lod2", //
		"mesh_lod3" //
	};
	for (const char *name : s_manual_properties) {
		if (p_path == name) {
			return true;
		}
	}
	return false;
}

} // namespace zylann::voxel
