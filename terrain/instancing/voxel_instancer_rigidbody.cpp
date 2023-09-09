#include "voxel_instancer_rigidbody.h"

namespace zylann::voxel {

VoxelInstancerRigidBody::VoxelInstancerRigidBody() {
	set_freeze_mode(RigidBody3D::FREEZE_MODE_STATIC);
	set_freeze_enabled(true);
}

int VoxelInstancerRigidBody::get_library_item_id() const {
	ERR_FAIL_COND_V(_parent == nullptr, -1);
	return _parent->get_library_item_id_from_render_block_index(_render_block_index);
}

void VoxelInstancerRigidBody::_notification(int p_what) {
	switch (p_what) {
		// TODO Optimization: this is also called when we quit the game or destroy the world
		// which can make things a bit slow, but I don't know if it can easily be avoided
		case NOTIFICATION_UNPARENTED:
			// The user could queue_free() that node in game,
			// so we have to notify the instancer to remove the multimesh instance and pointer
			if (_parent != nullptr) {
				_parent->on_body_removed(_data_block_position, _render_block_index, _instance_index);
				_parent = nullptr;
			}
			break;
	}
}

void VoxelInstancerRigidBody::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_library_item_id"), &VoxelInstancerRigidBody::get_library_item_id);
}

} // namespace zylann::voxel
