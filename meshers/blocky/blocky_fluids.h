#ifndef VOXEL_BLOCKY_FLUIDS_H
#define VOXEL_BLOCKY_FLUIDS_H

#include "voxel_blocky_library.h"
#include "voxel_blocky_model.h"

namespace zylann::voxel::blocky {

void generate_preview_fluid_model(
		const VoxelBlockyModel::BakedData &model,
		const uint16_t model_id,
		const VoxelBlockyLibraryBase::BakedData &library,
		Span<const VoxelBlockyModel::Surface> &out_model_surfaces,
		const FixedArray<FixedArray<VoxelBlockyModel::SideSurface, VoxelBlockyModel::MAX_SURFACES>, Cube::SIDE_COUNT> *
				&out_model_sides_surfaces
);

} // namespace zylann::voxel::blocky

#endif // VOXEL_BLOCKY_FLUIDS_H
