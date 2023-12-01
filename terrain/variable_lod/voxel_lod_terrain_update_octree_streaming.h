#ifndef VOXEL_LOD_TERRAIN_UPDATE_OCTREE_STREAMING_H
#define VOXEL_LOD_TERRAIN_UPDATE_OCTREE_STREAMING_H

#include "voxel_lod_terrain_update_data.h"

namespace zylann::voxel {

class VoxelData;

void process_octree_streaming(VoxelLodTerrainUpdateData::State &state, VoxelData &data, Vector3 viewer_pos,
		std::vector<VoxelData::BlockToSave> &data_blocks_to_save,
		std::vector<VoxelLodTerrainUpdateData::BlockLocation> &data_blocks_to_load,
		const VoxelLodTerrainUpdateData::Settings &settings, Ref<VoxelStream> stream, bool stream_enabled);

} // namespace zylann::voxel

#endif // VOXEL_LOD_TERRAIN_UPDATE_OCTREE_STREAMING_H
