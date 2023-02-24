#ifndef VOXEL_INSTANCE_LIBRARY_MULTIMESH_ITEM_EDITOR_PLUGIN_H
#define VOXEL_INSTANCE_LIBRARY_MULTIMESH_ITEM_EDITOR_PLUGIN_H

#include "../../terrain/instancing/voxel_instance_library_multimesh_item.h"
#include "../../util/godot/classes/editor_plugin.h"
#include "voxel_instance_library_multimesh_item_inspector_plugin.h"

ZN_GODOT_FORWARD_DECLARE(class EditorFileDialog)

namespace zylann::voxel {

class VoxelInstanceLibraryMultiMeshItemEditorPlugin : public ZN_EditorPlugin {
	GDCLASS(VoxelInstanceLibraryMultiMeshItemEditorPlugin, ZN_EditorPlugin)
public:
	VoxelInstanceLibraryMultiMeshItemEditorPlugin();

#if defined(ZN_GODOT)
	void _on_update_from_scene_button_pressed(VoxelInstanceLibraryMultiMeshItem *item);
#elif defined(ZN_GODOT_EXTENSION)
	void _on_update_from_scene_button_pressed(Object *item_o);
#endif

protected:
	bool _zn_handles(const Object *p_object) const override;
	void _zn_edit(Object *p_object) override;
	void _zn_make_visible(bool visible) override;

private:
	void _notification(int p_what);

	void _on_open_scene_dialog_file_selected(String fpath);

	static void _bind_methods();

	EditorFileDialog *_open_scene_dialog = nullptr;
	Ref<VoxelInstanceLibraryMultiMeshItem> _item;
	Ref<VoxelInstanceLibraryMultiMeshItemInspectorPlugin> _inspector_plugin;
};

} // namespace zylann::voxel

#endif // VOXEL_INSTANCE_LIBRARY_MULTIMESH_ITEM_EDITOR_PLUGIN_H
