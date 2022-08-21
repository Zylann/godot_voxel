#ifndef VOXEL_INSTANCE_LIBRARY_MULTIMESH_ITEM_EDITOR_PLUGIN_H
#define VOXEL_INSTANCE_LIBRARY_MULTIMESH_ITEM_EDITOR_PLUGIN_H

#include "../../terrain/instancing/voxel_instance_library_multimesh_item.h"
#include "voxel_instance_library_multimesh_item_inspector_plugin.h"
#include <editor/editor_plugin.h>

namespace zylann::voxel {

class VoxelInstanceLibraryMultiMeshItemEditorPlugin : public EditorPlugin {
	GDCLASS(VoxelInstanceLibraryMultiMeshItemEditorPlugin, EditorPlugin)
public:
	VoxelInstanceLibraryMultiMeshItemEditorPlugin();

	bool handles(Object *p_object) const override;
	void edit(Object *p_object) override;
	void make_visible(bool visible) override;

	void _on_update_from_scene_button_pressed(VoxelInstanceLibraryMultiMeshItem *item);

private:
	void _notification(int p_what);

	void _on_open_scene_dialog_file_selected(String fpath);

	EditorFileDialog *_open_scene_dialog = nullptr;
	Ref<VoxelInstanceLibraryMultiMeshItem> _item;
	Ref<VoxelInstanceLibraryMultiMeshItemInspectorPlugin> _inspector_plugin;
};

} // namespace zylann::voxel

#endif // VOXEL_INSTANCE_LIBRARY_MULTIMESH_ITEM_EDITOR_PLUGIN_H
