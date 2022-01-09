#include "voxel_server_updater.h"
#include "../util/macros.h"
#include "voxel_server.h"

// Needed for doing `Node *root = SceneTree::get_root()`, Window* is forward-declared
#include <scene/main/window.h>

namespace {
bool g_updater_created = false;
}

VoxelServerUpdater::VoxelServerUpdater() {
	PRINT_VERBOSE("Creating VoxelServerUpdater");
	set_process(true);
	g_updater_created = true;
}

VoxelServerUpdater::~VoxelServerUpdater() {
	g_updater_created = false;
}

void VoxelServerUpdater::ensure_existence(SceneTree *st) {
	if (st == nullptr) {
		return;
	}
	if (g_updater_created) {
		return;
	}
	Node *root = st->get_root();
	for (int i = 0; i < root->get_child_count(); ++i) {
		VoxelServerUpdater *u = Object::cast_to<VoxelServerUpdater>(root->get_child(i));
		if (u != nullptr) {
			return;
		}
	}
	VoxelServerUpdater *u = memnew(VoxelServerUpdater);
	u->set_name("VoxelServerUpdater_dont_touch_this");
	root->add_child(u);
}

void VoxelServerUpdater::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_PROCESS:
			// To workaround the absence of API to have a custom server processing in the main loop
			zylann::voxel::VoxelServer::get_singleton()->process();
			break;

		case NOTIFICATION_PREDELETE:
			PRINT_VERBOSE("Deleting VoxelServerUpdater");
			break;

		default:
			break;
	}
}
