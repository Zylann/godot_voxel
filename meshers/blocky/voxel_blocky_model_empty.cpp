#include "voxel_blocky_model_empty.h"
#include "blocky_model_baking_context.h"

namespace zylann::voxel {

VoxelBlockyModelEmpty::VoxelBlockyModelEmpty() {
	set_collision_aabbs(Span<const AABB>());
}

void VoxelBlockyModelEmpty::bake(blocky::ModelBakingContext &ctx) const {
	ctx.model.clear();
	VoxelBlockyModel::bake(ctx);
}

void VoxelBlockyModelEmpty::rotate_90(math::Axis axis, bool clockwise) {
	rotate_collision_boxes_90(axis, clockwise);
}

Ref<Mesh> VoxelBlockyModelEmpty::get_preview_mesh() const {
	return Ref<Mesh>();
}

bool VoxelBlockyModelEmpty::is_empty() const {
	return true;
}

} // namespace zylann::voxel
