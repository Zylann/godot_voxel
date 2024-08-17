#ifndef VOXEL_LOD_TERRAIN_UPDATE_OCTREE_STREAMING_H
#define VOXEL_LOD_TERRAIN_UPDATE_OCTREE_STREAMING_H

#include "../../storage/voxel_data.h"
#include "voxel_lod_terrain_update_data.h"

namespace zylann::voxel {

// Limitations:
// - Supports only one viewer
// - Still assumes a viewer exists at world origin if there is actually no viewer
// - Does not support viewer flags (separate collision/visual/voxel requirements)

void process_octree_streaming(VoxelLodTerrainUpdateData::State &state, VoxelData &data, Vector3 viewer_pos,
		StdVector<VoxelData::BlockToSave> *data_blocks_to_save,
		StdVector<VoxelLodTerrainUpdateData::BlockToLoad> &data_blocks_to_load,
		const VoxelLodTerrainUpdateData::Settings &settings, bool stream_enabled);

} // namespace zylann::voxel

#endif // VOXEL_LOD_TERRAIN_UPDATE_OCTREE_STREAMING_H
