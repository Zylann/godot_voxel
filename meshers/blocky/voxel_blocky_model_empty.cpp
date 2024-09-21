#include "voxel_blocky_model_empty.h"

namespace zylann::voxel {

VoxelBlockyModelEmpty::VoxelBlockyModelEmpty() {
	set_collision_aabbs(Span<const AABB>());
}

void VoxelBlockyModelEmpty::bake(BakedData &baked_data, bool bake_tangents, MaterialIndexer &materials) const {
	baked_data.clear();
	VoxelBlockyModel::bake(baked_data, bake_tangents, materials);
}

Ref<Mesh> VoxelBlockyModelEmpty::get_preview_mesh() const {
	return Ref<Mesh>();
}

bool VoxelBlockyModelEmpty::is_empty() const {
	return true;
}

} // namespace zylann::voxel
