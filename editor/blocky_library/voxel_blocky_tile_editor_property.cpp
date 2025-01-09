#include "voxel_blocky_tile_editor_property.h"
#include "../../meshers/blocky/voxel_blocky_texture_atlas.h"
#include "../../util/godot/classes/atlas_texture.h"
#include "../../util/godot/editor_scale.h"
#include "../blocky_texture_atlas/voxel_blocky_texture_atlas_editor.h"
#include "voxel_blocky_library_editor_plugin.h"

namespace zylann::voxel {

VoxelBlockyTileEditorProperty::VoxelBlockyTileEditorProperty() {
	_button = memnew(Button);
	_button->set_expand_icon(true);
	_button->connect("pressed", callable_mp(this, &VoxelBlockyTileEditorProperty::on_button_pressed));
	add_child(_button);
}

void VoxelBlockyTileEditorProperty::setup(
		const Object *obj,
		StringName atlas_property,
		const int tile_id,
		VoxelBlockyLibraryEditorPlugin *plugin
) {
	_atlas_property = atlas_property;
	_plugin = plugin;
	ERR_FAIL_COND(obj == nullptr);
	update_button(*obj, tile_id);
}

void VoxelBlockyTileEditorProperty::_zn_set_read_only(bool p_read_only) {
	_button->set_disabled(p_read_only);
}

void VoxelBlockyTileEditorProperty::_zn_update_property() {
	const int tile_id = get_edited_property_value();

	Object *obj = get_edited_object();
	ERR_FAIL_COND(obj == nullptr);
	update_button(*obj, tile_id);
}

void VoxelBlockyTileEditorProperty::update_button(const Object &obj, const int tile_id) {
	String title;
	String tooltip;
	Ref<AtlasTexture> icon;

	if (tile_id < 0) {
		title = "<no tile>";
		tooltip = "[-1]";
	} else {
		Ref<VoxelBlockyTextureAtlas> atlas = obj.get(_atlas_property);
		if (atlas.is_valid()) {
			title = atlas->get_tile_name_or_default(tile_id);
			tooltip = String("[{0}]").format(varray(tile_id));
			icon = atlas->get_tile_preview_texture(tile_id);
		} else {
			// Should probably not happen? When an atlas is not set, this property editor changes
			title = "<no atlas>";
		}
	}

	// TODO Button icon showing a preview of the tile?

	_button->set_text(title);
	_button->set_button_icon(icon);
	_button->set_tooltip_text(tooltip);
}

void VoxelBlockyTileEditorProperty::on_button_pressed() {
	Object *obj = get_edited_object();
	ZN_ASSERT_RETURN(obj != nullptr);
	Ref<VoxelBlockyTextureAtlas> atlas = obj->get(_atlas_property);
	if (atlas.is_null()) {
		return;
	}
	_plugin->open_atlas_tile_picker(atlas, this);
}

void VoxelBlockyTileEditorProperty::on_atlas_tile_picker_confirmed(const int selected_tile_id) {
	if (selected_tile_id < 0) {
		ZN_PRINT_VERBOSE("No tile selected");
		return;
	}

	emit_changed(get_edited_property(), selected_tile_id, "");
}

void VoxelBlockyTileEditorProperty::_bind_methods() {
	//
}

} // namespace zylann::voxel
