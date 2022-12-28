#include "voxel_engine_updater.h"
#include "../util/log.h"
#include "voxel_engine.h"

// Needed for doing `Node *root = SceneTree::get_root()`, Window* is forward-declared
#include "../util/godot/classes/scene_tree.h"
#include "../util/godot/classes/window.h"

namespace zylann::voxel {

bool g_updater_created = false;

VoxelEngineUpdater::VoxelEngineUpdater() {
	ZN_PRINT_VERBOSE("Creating VoxelEngineUpdater");
	set_process(true);
	g_updater_created = true;
}

VoxelEngineUpdater::~VoxelEngineUpdater() {
	g_updater_created = false;
}

void VoxelEngineUpdater::ensure_existence(SceneTree *st) {
	if (st == nullptr) {
		return;
	}
	if (g_updater_created) {
		return;
	}
	Node *root = st->get_root();
	for (int i = 0; i < root->get_child_count(); ++i) {
		VoxelEngineUpdater *u = Object::cast_to<VoxelEngineUpdater>(root->get_child(i));
		if (u != nullptr) {
			return;
		}
	}
	VoxelEngineUpdater *u = memnew(VoxelEngineUpdater);
	u->set_name("VoxelEngineUpdater_dont_touch_this");
	// TODO This can fail (for example if `Node::data.blocked > 0` while in `_ready()`) but Godot offers no API to check
	// anything. So if this fail, the node will leak.
	root->add_child(u);
}

void VoxelEngineUpdater::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_PROCESS:
			// To workaround the absence of API to have a custom server processing in the main loop
			zylann::voxel::VoxelEngine::get_singleton().process();
			break;

		case NOTIFICATION_PREDELETE:
			ZN_PRINT_VERBOSE("Deleting VoxelEngineUpdater");
			break;

		default:
			break;
	}
}

} // namespace zylann::voxel
