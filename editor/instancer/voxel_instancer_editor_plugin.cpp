#include "voxel_instancer_editor_plugin.h"
#include "../../terrain/instancing/voxel_instancer.h"

namespace zylann::voxel {

VoxelInstancerEditorPlugin::VoxelInstancerEditorPlugin() {}

bool VoxelInstancerEditorPlugin::handles(Object *p_object) const {
	ERR_FAIL_COND_V(p_object == nullptr, false);
	return Object::cast_to<VoxelInstancer>(p_object) != nullptr;
}

void VoxelInstancerEditorPlugin::edit(Object *p_object) {
	VoxelInstancer *instancer = Object::cast_to<VoxelInstancer>(p_object);
	ERR_FAIL_COND(instancer == nullptr);
	instancer->debug_set_draw_enabled(true);
	instancer->debug_set_draw_flag(VoxelInstancer::DEBUG_DRAW_ALL_BLOCKS, true);
	_node = instancer;
}

void VoxelInstancerEditorPlugin::make_visible(bool visible) {
	if (visible == false) {
		if (_node != nullptr) {
			_node->debug_set_draw_enabled(false);
			_node = nullptr;
		}
	}
}

} // namespace zylann::voxel
