#ifndef VOXEL_BLOCKY_TILE_EDITOR_PROPERTY_H
#define VOXEL_BLOCKY_TILE_EDITOR_PROPERTY_H

#include "../../util/godot/classes/editor_property.h"
#include "../../util/godot/macros.h"
//
ZN_GODOT_FORWARD_DECLARE(class Button);
ZN_GODOT_FORWARD_DECLARE(class ConfirmationDialog);

namespace zylann::voxel {

class VoxelBlockyTextureAtlasEditor;
class VoxelBlockyLibraryEditorPlugin;

// Inspector control for a property referencing a tile of a VoxelBlockyTextureAtlas.
// When clicked, opens a dialog to pick a tile from the atlas.
class VoxelBlockyTileEditorProperty : public zylann::godot::ZN_EditorProperty {
	GDCLASS(VoxelBlockyTileEditorProperty, zylann::godot::ZN_EditorProperty);

public:
	VoxelBlockyTileEditorProperty();

	void setup(const Object *obj, StringName atlas_property, const int tile_id, VoxelBlockyLibraryEditorPlugin *plugin);
	void on_atlas_tile_picker_confirmed(const int selected_tile_id);

protected:
	void _zn_set_read_only(bool p_read_only) override;
	void _zn_update_property() override;

	void on_button_pressed();

	void update_button(const Object &obj, const int tile_id);

private:
	static void _bind_methods();

	Button *_button = nullptr;
	Ref<Resource> _resource;
	StringName _atlas_property;
	ConfirmationDialog *_dialog = nullptr;
	VoxelBlockyTextureAtlasEditor *_atlas_editor = nullptr;
	VoxelBlockyLibraryEditorPlugin *_plugin = nullptr;
};

} // namespace zylann::voxel

#endif // VOXEL_BLOCKY_TILE_EDITOR_PROPERTY_H
