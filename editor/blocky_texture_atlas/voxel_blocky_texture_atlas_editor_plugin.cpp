#include "voxel_blocky_texture_atlas_editor_plugin.h"
#include "../../meshers/blocky/voxel_blocky_texture_atlas.h"
#include "../../util/godot/classes/button.h"
#include "../../util/godot/editor_scale.h"
#include "voxel_blocky_texture_atlas_editor.h"

#if defined(ZN_GODOT)
// This is exceptionally necessary because of the default argument of `add_control_to_bottom_panel`.
// Very confusing because MSVC didn't point at that line, instead pointing at `editor_context_menu_plugin.h(67)`
#include "../../util/godot/classes/shortcut.h"
#endif

namespace zylann::voxel {

VoxelBlockyTextureAtlasEditorPlugin::VoxelBlockyTextureAtlasEditorPlugin() {
	const float editor_scale = EDSCALE;
	_editor = memnew(VoxelBlockyTextureAtlasEditor);
	_editor->set_custom_minimum_size(Vector2(100, 100) * editor_scale);
	_bottom_panel_button = add_control_to_bottom_panel(_editor, "Voxel Atlas");
	_bottom_panel_button->hide();
}

bool VoxelBlockyTextureAtlasEditorPlugin::_zn_handles(const Object *p_object) const {
	return Object::cast_to<VoxelBlockyTextureAtlas>(p_object) != nullptr;
}

void VoxelBlockyTextureAtlasEditorPlugin::_zn_edit(Object *p_object) {
	Ref<VoxelBlockyTextureAtlas> atlas(Object::cast_to<VoxelBlockyTextureAtlas>(p_object));
	_editor->set_atlas(atlas);
}

void VoxelBlockyTextureAtlasEditorPlugin::_zn_make_visible(bool visible) {
	if (visible) {
		_bottom_panel_button->show();
		make_bottom_panel_item_visible(_editor);

	} else {
		if (_editor->is_visible_in_tree()) {
			hide_bottom_panel();
		}
		_bottom_panel_button->hide();
	}
}

} // namespace zylann::voxel
