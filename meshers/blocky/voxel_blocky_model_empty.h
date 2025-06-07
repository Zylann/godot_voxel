#ifndef VOXEL_BLOCKY_MODEL_EMPTY_H
#define VOXEL_BLOCKY_MODEL_EMPTY_H

#include "voxel_blocky_model.h"

namespace zylann::voxel {

// A model with no visuals and no collisions by default.
class VoxelBlockyModelEmpty : public VoxelBlockyModel {
	GDCLASS(VoxelBlockyModelEmpty, VoxelBlockyModel)
public:
	VoxelBlockyModelEmpty();

	void bake(blocky::ModelBakingContext &ctx) const override;

	Ref<Mesh> get_preview_mesh() const override;
	bool is_empty() const override;

private:
	static void _bind_methods() {}
};

} // namespace zylann::voxel

#endif // VOXEL_BLOCKY_MODEL_EMPTY_H
