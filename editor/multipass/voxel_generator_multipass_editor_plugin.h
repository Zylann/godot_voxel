#ifndef VOXEL_GENERATOR_MULTIPASS_EDITOR_PLUGIN_H
#define VOXEL_GENERATOR_MULTIPASS_EDITOR_PLUGIN_H

#include "../../util/godot/classes/editor_plugin.h"
#include "voxel_generator_multipass_editor_inspector_plugin.h"

namespace zylann::voxel {

class VoxelGeneratorMultipassEditorPlugin : public zylann::godot::ZN_EditorPlugin {
	GDCLASS(VoxelGeneratorMultipassEditorPlugin, zylann::godot::ZN_EditorPlugin)
public:
	VoxelGeneratorMultipassEditorPlugin();

private:
	void _notification(int p_what);

	// When compiling with GodotCpp, `_bind_methods` is not optional.
	static void _bind_methods() {}

	Ref<VoxelGeneratorMultipassEditorInspectorPlugin> _inspector_plugin;
};

} // namespace zylann::voxel

#endif // VOXEL_GENERATOR_MULTIPASS_EDITOR_PLUGIN_H
