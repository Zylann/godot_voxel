#ifndef VOXEL_INSTANCER_EDITOR_PLUGIN_H
#define VOXEL_INSTANCER_EDITOR_PLUGIN_H

#include "../../util/godot/classes/editor_plugin.h"
#include "../../util/godot/macros.h"

ZN_GODOT_FORWARD_DECLARE(class MenuButton)

namespace zylann::voxel {

class VoxelInstancer;
class VoxelInstancerStatView;

class VoxelInstancerEditorPlugin : public zylann::godot::ZN_EditorPlugin {
	GDCLASS(VoxelInstancerEditorPlugin, zylann::godot::ZN_EditorPlugin)
public:
	VoxelInstancerEditorPlugin();

protected:
	bool _zn_handles(const Object *p_object) const override;
	void _zn_edit(Object *p_object) override;
	void _zn_make_visible(bool visible) override;

private:
	void init();
	void _notification(int p_what);
	bool toggle_stat_view();
	void _on_menu_item_selected(int id);

	VoxelInstancer *get_instancer();

	static void _bind_methods();

	MenuButton *_menu_button = nullptr;
	// Using an ObjectID for referencing, necause it's a neverending struggle to keep checking pointer validity.
	// When closing a scene while the node is selected, Godot will call `make_visible(false)` and `edit(null)` AFTER
	// having deleted all the nodes, which means this plugin will be left with a dangling pointer when it's time to
	// turn off the node's debug drawing feature...
	ObjectID _instancer_object_id = ObjectID();
	VoxelInstancerStatView *_stat_view = nullptr;
};

} // namespace zylann::voxel

#endif // VOXEL_INSTANCER_EDITOR_PLUGIN_H
