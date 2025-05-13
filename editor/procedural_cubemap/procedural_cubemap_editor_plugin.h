#ifndef VOXEL_PROCEDURAL_CUBEMAP_EDITOR_PLUGIN_H
#define VOXEL_PROCEDURAL_CUBEMAP_EDITOR_PLUGIN_H

#include "../../util/godot/classes/editor_plugin.h"
#include "../../util/godot/macros.h"

namespace zylann::voxel {

class VoxelProceduralCubemapEditorPlugin : public zylann::godot::ZN_EditorPlugin {
	GDCLASS(VoxelProceduralCubemapEditorPlugin, zylann::godot::ZN_EditorPlugin)
public:
	VoxelProceduralCubemapEditorPlugin();

private:
	void init();
	void _notification(int p_what);

	static void _bind_methods();
};

} // namespace zylann::voxel

#endif // VOXEL_PROCEDURAL_CUBEMAP_EDITOR_PLUGIN_H
