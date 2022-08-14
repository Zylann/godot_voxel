#ifndef VOXEL_INSTANCER_EDITOR_PLUGIN_H
#define VOXEL_INSTANCER_EDITOR_PLUGIN_H

#include <editor/editor_plugin.h>

namespace zylann::voxel {

class VoxelInstancer;
class VoxelInstancerStatView;

class VoxelInstancerEditorPlugin : public EditorPlugin {
	GDCLASS(VoxelInstancerEditorPlugin, EditorPlugin)
public:
	VoxelInstancerEditorPlugin();

	bool handles(Object *p_object) const override;
	void edit(Object *p_object) override;
	void make_visible(bool visible) override;

private:
	bool toggle_stat_view();
	void _on_menu_item_selected(int id);

	MenuButton *_menu_button = nullptr;
	VoxelInstancer *_node = nullptr;
	VoxelInstancerStatView *_stat_view = nullptr;
};

} // namespace zylann::voxel

#endif // VOXEL_INSTANCER_EDITOR_PLUGIN_H
