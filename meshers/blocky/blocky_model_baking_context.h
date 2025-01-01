#ifndef VOXEL_BLOCKY_MODEL_BAKING_CONTEXT_H
#define VOXEL_BLOCKY_MODEL_BAKING_CONTEXT_H

#include "../../util/containers/std_vector.h"
#include "voxel_blocky_fluid.h"
#include "voxel_blocky_model.h"

namespace zylann::voxel {
namespace blocky {

struct MaterialIndexer;

struct ModelBakingContext {
	VoxelBlockyModel::BakedData &model;
	// const uint16_t model_index;
	const bool tangents_enabled;
	MaterialIndexer &material_indexer;
	StdVector<Ref<VoxelBlockyFluid>> &indexed_fluids;
	StdVector<VoxelBlockyFluid::BakedData> &baked_fluids;
};

} // namespace blocky
} // namespace zylann::voxel

#endif // VOXEL_BLOCKY_MODEL_BAKING_CONTEXT_H
