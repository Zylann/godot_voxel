#ifndef VOXEL_INSTANCER_EDITOR_PLUGIN_H
#define VOXEL_INSTANCER_EDITOR_PLUGIN_H

#include "../../util/godot/classes/editor_plugin.h"
#include "../../util/macros.h"

ZN_GODOT_FORWARD_DECLARE(class MenuButton)

namespace zylann::voxel {

class VoxelInstancer;
class VoxelInstancerStatView;

class VoxelInstancerEditorPlugin : public EditorPlugin {
	GDCLASS(VoxelInstancerEditorPlugin, EditorPlugin)
public:
	VoxelInstancerEditorPlugin();

#if defined(ZN_GODOT)
	bool handles(Object *p_object) const override;
	void edit(Object *p_object) override;
	void make_visible(bool visible) override;
#elif defined(ZN_GODOT_EXTENSION)
	bool _handles(const Variant &p_object_v) const override;
	void _edit(const Variant &p_object_v) override;
	void _make_visible(bool visible) override;
#endif

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
