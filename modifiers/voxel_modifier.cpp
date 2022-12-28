#include "voxel_modifier.h"

namespace zylann::voxel {

void VoxelModifier::set_transform(Transform3D t) {
	RWLockWrite wlock(_rwlock);
	if (t == _transform) {
		return;
	}
	_transform = t;
	_shader_data_need_update = true;
	update_aabb();
}

} // namespace zylann::voxel
