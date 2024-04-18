#ifndef VOXEL_LOD_TERRAIN_UPDATE_CLIPBOX_STREAMING_H
#define VOXEL_LOD_TERRAIN_UPDATE_CLIPBOX_STREAMING_H

#include "../../storage/voxel_data.h"
#include "voxel_lod_terrain_update_data.h"

namespace zylann::voxel {

void process_clipbox_streaming( //
		VoxelLodTerrainUpdateData::State &state, //
		VoxelData &data, //
		Span<const std::pair<ViewerID, VoxelEngine::Viewer>> viewers, //
		const Transform3D &volume_transform, //
		StdVector<VoxelData::BlockToSave> *data_blocks_to_save, //
		StdVector<VoxelLodTerrainUpdateData::BlockToLoad> &data_blocks_to_load, //
		const VoxelLodTerrainUpdateData::Settings &settings, //
		bool can_load, //
		bool can_mesh //
);

} // namespace zylann::voxel

#endif // VOXEL_LOD_TERRAIN_UPDATE_CLIPBOX_STREAMING_H
