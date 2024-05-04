#ifndef VOXEL_BLOCKY_MODEL_BAKING_CONTEXT_H
#define VOXEL_BLOCKY_MODEL_BAKING_CONTEXT_H

#include "voxel_blocky_model.h"

namespace zylann::voxel::blocky {

struct ModelBakingContext {
	VoxelBlockyModel::BakedData &model;
	bool tangents_enabled;
	VoxelBlockyModel::MaterialIndexer &material_indexer;
};

} // namespace zylann::voxel::blocky

#endif // VOXEL_BLOCKY_MODEL_BAKING_CONTEXT_H
