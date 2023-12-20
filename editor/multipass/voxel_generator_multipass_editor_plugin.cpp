#include "voxel_generator_multipass_editor_plugin.h"

namespace zylann::voxel {

VoxelGeneratorMultipassEditorPlugin::VoxelGeneratorMultipassEditorPlugin() {}

void VoxelGeneratorMultipassEditorPlugin::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE) {
		_inspector_plugin.instantiate();
		// TODO Why can other Godot plugins do this in the constructor??
		// I found I could not put this in the constructor,
		// otherwise `add_inspector_plugin` causes ANOTHER editor plugin to leak on exit... Oo
		add_inspector_plugin(_inspector_plugin);

	} else if (p_what == NOTIFICATION_EXIT_TREE) {
		remove_inspector_plugin(_inspector_plugin);
	}
}

} // namespace zylann::voxel
