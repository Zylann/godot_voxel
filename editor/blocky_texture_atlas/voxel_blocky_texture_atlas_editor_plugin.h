#ifndef VOXEL_BLOCKY_TEXTURE_ATLAS_EDITOR_PLUGIN_H
#define VOXEL_BLOCKY_TEXTURE_ATLAS_EDITOR_PLUGIN_H

#include "../../util/godot/classes/editor_plugin.h"

namespace zylann::voxel {

class VoxelBlockyTextureAtlasEditor;

class VoxelBlockyTextureAtlasEditorPlugin : public zylann::godot::ZN_EditorPlugin {
	GDCLASS(VoxelBlockyTextureAtlasEditorPlugin, zylann::godot::ZN_EditorPlugin)
public:
	VoxelBlockyTextureAtlasEditorPlugin();

protected:
	bool _zn_handles(const Object *p_object) const override;
	void _zn_edit(Object *p_object) override;
	void _zn_make_visible(bool visible) override;

private:
	static void _bind_methods() {}

	VoxelBlockyTextureAtlasEditor *_editor = nullptr;
	Button *_bottom_panel_button = nullptr;
};

} // namespace zylann::voxel

#endif // VOXEL_BLOCKY_TEXTURE_ATLAS_EDITOR_PLUGIN_H
