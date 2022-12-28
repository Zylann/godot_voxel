#ifndef VOXEL_INSTANCE_LIBRARY_MULTIMESH_ITEM_EDITOR_PLUGIN_H
#define VOXEL_INSTANCE_LIBRARY_MULTIMESH_ITEM_EDITOR_PLUGIN_H

#include "../../terrain/instancing/voxel_instance_library_multimesh_item.h"
#include "../../util/godot/classes/editor_plugin.h"
#include "voxel_instance_library_multimesh_item_inspector_plugin.h"

ZN_GODOT_FORWARD_DECLARE(class EditorFileDialog)

namespace zylann::voxel {

class VoxelInstanceLibraryMultiMeshItemEditorPlugin : public EditorPlugin {
	GDCLASS(VoxelInstanceLibraryMultiMeshItemEditorPlugin, EditorPlugin)
public:
	VoxelInstanceLibraryMultiMeshItemEditorPlugin();

#if defined(ZN_GODOT)
	bool handles(Object *p_object) const override;
	void edit(Object *p_object) override;
	void make_visible(bool visible) override;
	void _on_update_from_scene_button_pressed(VoxelInstanceLibraryMultiMeshItem *item);
#elif defined(ZN_GODOT_EXTENSION)
	bool _handles(const Variant &p_object_v) const override;
	void _edit(const Variant &p_object) override;
	void _make_visible(bool visible) override;
	// TODO GDX: Cannot register methods taking a child class of `Object*`
	void _on_update_from_scene_button_pressed(Object *item_o);
#endif

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
