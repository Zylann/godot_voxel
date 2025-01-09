#include "voxel_blocky_library_editor_plugin.h"
#include "../../util/godot/classes/control.h"
#include "../../util/godot/classes/editor_interface.h"
#include "../../util/godot/editor_scale.h"
#include "../blocky_texture_atlas/voxel_blocky_texture_atlas_editor.h"
#include "types/voxel_blocky_type_editor_inspector_plugin.h"
#include "types/voxel_blocky_type_library_editor_inspector_plugin.h"
#include "types/voxel_blocky_type_library_ids_dialog.h"
#include "voxel_blocky_model_editor_inspector_plugin.h"
#include "voxel_blocky_tile_editor_property.h"
#include "voxel_blocky_tile_selection_inspector_plugin.h"

namespace zylann::voxel {

VoxelBlockyLibraryEditorPlugin::VoxelBlockyLibraryEditorPlugin() {}

// TODO GDX: Can't initialize EditorPlugins in their constructor when they access EditorNode.
// See https://github.com/godotengine/godot-cpp/issues/1179
void VoxelBlockyLibraryEditorPlugin::init() {
	EditorUndoRedoManager *undo_redo = get_undo_redo();
	EditorInterface *editor_interface = get_editor_interface();

	// Models
	{
		Ref<VoxelBlockyModelEditorInspectorPlugin> plugin;
		plugin.instantiate();
		plugin->set_undo_redo(undo_redo);
		add_inspector_plugin(plugin);
	}

	// Types
	{
		Ref<VoxelBlockyTypeEditorInspectorPlugin> plugin;
		plugin.instantiate();
		plugin->set_editor_interface(editor_interface);
		plugin->set_undo_redo(undo_redo);
		add_inspector_plugin(plugin);
	}

	Control *base_control = editor_interface->get_base_control();

	// Tiles
	{
		_inspector_atlas_picker = memnew(VoxelBlockyTextureAtlasEditor);
		_inspector_atlas_picker->make_read_only();

		const float edscale = EDSCALE;

		_inspector_atlas_picker_dialog = memnew(ConfirmationDialog);
		_inspector_atlas_picker_dialog->add_child(_inspector_atlas_picker);
		_inspector_atlas_picker_dialog->set_size(Vector2(500, 300) * edscale);
		_inspector_atlas_picker_dialog->set_title("Select Tile...");
		_inspector_atlas_picker_dialog->connect(
				"confirmed",
				callable_mp(this, &VoxelBlockyLibraryEditorPlugin::on_inspector_atlas_picker_dialog_confirmed)
		);
		base_control->add_child(_inspector_atlas_picker_dialog);
	}
	{
		Ref<VoxelBlockyTileSelectionInspectorPlugin> plugin;
		plugin.instantiate();
		plugin->set_plugin(this);
		add_inspector_plugin(plugin);
	}

	_type_library_ids_dialog = memnew(VoxelBlockyTypeLibraryIDSDialog);
	base_control->add_child(_type_library_ids_dialog);

	{
		Ref<VoxelBlockyTypeLibraryEditorInspectorPlugin> plugin;
		plugin.instantiate();
		plugin->set_ids_dialog(_type_library_ids_dialog);
		add_inspector_plugin(plugin);
	}
}

void VoxelBlockyLibraryEditorPlugin::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE) {
		init();
	}
}

void VoxelBlockyLibraryEditorPlugin::open_atlas_tile_picker(
		Ref<VoxelBlockyTextureAtlas> atlas,
		VoxelBlockyTileEditorProperty *property
) {
	_calling_property.set(property);
	_inspector_atlas_picker->set_atlas(atlas);
	_inspector_atlas_picker_dialog->popup_centered();
}

void VoxelBlockyLibraryEditorPlugin::on_inspector_atlas_picker_dialog_confirmed() {
	VoxelBlockyTileEditorProperty *property = _calling_property.get();
	ZN_ASSERT_RETURN(property != nullptr);
	const int selected_tile = _inspector_atlas_picker->get_selected_tile_id();
	property->on_atlas_tile_picker_confirmed(selected_tile);
}

} // namespace zylann::voxel
