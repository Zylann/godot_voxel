#include "voxel_blocky_tile_selection_inspector_plugin.h"
#include "../../constants/voxel_string_names.h"
#include "../../meshers/blocky/voxel_blocky_model_cube.h"
#include "../../meshers/blocky/voxel_blocky_texture_atlas.h"
#include "voxel_blocky_tile_editor_property.h"

namespace zylann::voxel {

void VoxelBlockyTileSelectionInspectorPlugin::set_plugin(VoxelBlockyLibraryEditorPlugin *plugin) {
	_plugin = plugin;
}

bool VoxelBlockyTileSelectionInspectorPlugin::_zn_can_handle(const Object *p_object) const {
	return Object::cast_to<VoxelBlockyModelCube>(p_object) != nullptr;
}

bool VoxelBlockyTileSelectionInspectorPlugin::_zn_parse_property(
		Object *p_object,
		const Variant::Type p_type,
		const String &p_path,
		const PropertyHint p_hint,
		const String &p_hint_text,
		const BitField<PropertyUsageFlags> p_usage,
		const bool p_wide
) {
	const VoxelBlockyModelCube *model = Object::cast_to<VoxelBlockyModelCube>(p_object);
	Ref<VoxelBlockyTextureAtlas> atlas = model->get_atlas();
	if (atlas.is_null()) {
		return false;
	}

	if (p_path.begins_with("atlas_tile_id_")) {
		const int tile_id = model->get(p_path);

		// Replace default editor with this one
		VoxelBlockyTileEditorProperty *ed = memnew(VoxelBlockyTileEditorProperty);
		ed->setup(p_object, VoxelStringNames::get_singleton().atlas, tile_id, _plugin);
		add_property_editor(p_path, ed);
		return true;
	}

	return false;
}

} // namespace zylann::voxel
