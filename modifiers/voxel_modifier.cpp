#include "voxel_modifier.h"
#include "../engine/gpu/gpu_task_runner.h"

namespace zylann::voxel {

void VoxelModifier::set_transform(Transform3D t) {
	RWLockWrite wlock(_rwlock);
	if (t == _transform) {
		return;
	}
	_transform = t;
#ifdef VOXEL_ENABLE_GPU
	_shader_data_need_update = true;
#endif
	update_aabb();
}

#ifdef VOXEL_ENABLE_GPU

RID VoxelModifier::get_detail_shader(const BaseGPUResources &base_resources, const Type type) {
	switch (type) {
		case TYPE_SPHERE:
			return base_resources.detail_modifier_sphere_shader.rid;
		case TYPE_MESH:
			return base_resources.detail_modifier_mesh_shader.rid;
		default:
			ZN_PRINT_ERROR("Unhandled modifier type");
			return RID();
	}
}

RID VoxelModifier::get_block_shader(const BaseGPUResources &base_resources, const Type type) {
	switch (type) {
		case TYPE_SPHERE:
			return base_resources.block_modifier_sphere_shader.rid;
		case TYPE_MESH:
			return base_resources.block_modifier_mesh_shader.rid;
		default:
			ZN_PRINT_ERROR("Unhandled modifier type");
			return RID();
	}
}

#endif

} // namespace zylann::voxel
