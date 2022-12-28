#ifndef VOX_EDITOR_PLUGIN_H
#define VOX_EDITOR_PLUGIN_H

#include "../../util/godot/classes/editor_plugin.h"
#include "vox_mesh_importer.h"
#include "vox_scene_importer.h"

namespace zylann::voxel::magica {

class VoxelVoxEditorPlugin : public EditorPlugin {
	GDCLASS(VoxelVoxEditorPlugin, EditorPlugin)
public:
	VoxelVoxEditorPlugin();

private:
	void _notification(int p_what);

	// When compiling with GodotCpp, `_bind_methods` is not optional.
	static void _bind_methods() {}

	Ref<VoxelVoxSceneImporter> _vox_scene_importer;
	Ref<VoxelVoxMeshImporter> _vox_mesh_importer;
};

} // namespace zylann::voxel::magica

#endif // VOX_EDITOR_PLUGIN_H
