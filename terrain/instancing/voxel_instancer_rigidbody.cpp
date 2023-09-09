#include "voxel_instancer_rigidbody.h"

namespace zylann::voxel {

int VoxelInstancerRigidBody::get_library_item_id() const {
	ERR_FAIL_COND_V(_parent == nullptr, -1);
	return _parent->get_library_item_id_from_render_block_index(_render_block_index);
}

void VoxelInstancerRigidBody::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_library_item_id"), &VoxelInstancerRigidBody::get_library_item_id);
}

} // namespace zylann::voxel
