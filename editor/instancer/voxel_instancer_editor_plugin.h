#ifndef VOXEL_INSTANCER_EDITOR_PLUGIN_H
#define VOXEL_INSTANCER_EDITOR_PLUGIN_H

#include "../../util/godot/classes/editor_plugin.h"
#include "../../util/macros.h"

ZN_GODOT_FORWARD_DECLARE(class MenuButton)

namespace zylann::voxel {

class VoxelInstancer;
class VoxelInstancerStatView;

class VoxelInstancerEditorPlugin : public ZN_EditorPlugin {
	GDCLASS(VoxelInstancerEditorPlugin, ZN_EditorPlugin)
public:
	VoxelInstancerEditorPlugin();

protected:
	bool _zn_handles(const Object *p_object) const override;
	void _zn_edit(Object *p_object) override;
	void _zn_make_visible(bool visible) override;

private:
	bool toggle_stat_view();
	void _on_menu_item_selected(int id);

	static void _bind_methods();

	MenuButton *_menu_button = nullptr;
	VoxelInstancer *_node = nullptr;
	VoxelInstancerStatView *_stat_view = nullptr;
};

} // namespace zylann::voxel

#endif // VOXEL_INSTANCER_EDITOR_PLUGIN_H
