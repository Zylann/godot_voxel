#ifndef VOXEL_TERRAIN_SYSTEM_H
#define VOXEL_TERRAIN_SYSTEM_H

#include "../providers/voxel_provider.h"
#include "voxel_data_loader.h"
#include "voxel_map.h"
#include "voxel_mesh_updater.h"
#include <vector>

// TODO Put terrain streaming system in one class and re-use it in nodes

class VoxelTerrainSystem {
public:
	static const int MAX_LOD = 32;

	enum BlockState {
		BLOCK_NONE, // There is no block
		BLOCK_LOAD, // The block is loading
		BLOCK_UPDATE_NOT_SENT, // The block needs an update but wasn't sent yet
		BLOCK_UPDATE_SENT, // The block needs an update which was sent
		BLOCK_IDLE // The block is up to date
	};

	struct Stats {
		VoxelMeshUpdater::Stats updater;
		VoxelStreamThread::Stats provider;
		uint64_t time_request_blocks_to_load = 0;
		uint64_t time_process_load_responses = 0;
		uint64_t time_request_blocks_to_update = 0;
		uint64_t time_process_update_responses = 0;
		uint64_t time_process_lod = 0;
		int blocked_lods = 0;
	};

private:
	Ref<VoxelStream> _stream;
	VoxelStreamThread *_stream_thread = nullptr;
	VoxelMeshUpdater *_block_updater = nullptr;
	std::vector<VoxelMeshUpdater::OutputBlock> _blocks_pending_main_thread_update;

	Ref<Material> _material;

	// Each LOD works in a set of coordinates spanning 2x more voxels the higher their index is
	struct Lod {
		Ref<VoxelMap> map;

		Map<Vector3i, BlockState> block_states;
		std::vector<Vector3i> blocks_pending_update;

		// Reflects LodOctree but only in this LOD
		// TODO Would be nice to use LodOctree directly!
		Set<Vector3i> blocks_in_meshing_area;

		// These are relative to this LOD, in block coordinates
		Vector3i last_viewer_block_pos;
		int last_view_distance_blocks = 0;

		// Members for memory caching
		std::vector<Vector3i> blocks_to_load;
	};

	Lod _lods[MAX_LOD];

	Stats _stats;
};

#endif // VOXEL_TERRAIN_SYSTEM_H
