#ifndef VOXEL_LOD_TERRAIN_UPDATE_DATA_H
#define VOXEL_LOD_TERRAIN_UPDATE_DATA_H

#include "../constants/voxel_constants.h"
#include "../generators/voxel_generator.h"
#include "../storage/voxel_data_map.h"
#include "../streams/voxel_stream.h"
#include "../util/fixed_array.h"
#include "lod_octree.h"
#include "voxel_mesh_map.h"

#include <unordered_set>

namespace zylann {

class AsyncDependencyTracker;

namespace voxel {

// struct VoxelMeshLodMap {
// 	struct Lod {
// 		VoxelMeshMap mesh_map;
// 		RWLock mesh_map_lock;
// 	};
// 	FixedArray<Lod, constants::MAX_LOD> lods;
// 	unsigned int lod_count;
// };

// Settings and states needed for the multi-threaded part of the update loop of VoxelLodTerrain.
// See `VoxelLodTerrainUpdateTask` for more info.
struct VoxelLodTerrainUpdateData {
	struct OctreeItem {
		LodOctree octree;
	};

	struct TransitionUpdate {
		Vector3i block_position;
		uint8_t transition_mask;
	};

	// These values don't change during the update task.
	struct Settings {
		// Area within which voxels can exist.
		// Note, these bounds might not be exactly represented. This volume is chunk-based, so the result will be
		// approximated to the closest chunk.
		Box3i bounds_in_voxels;
		unsigned int lod_count = 0;
		// Distance between a viewer and the end of LOD0
		float lod_distance = 0.f;
		unsigned int view_distance_voxels = 512;
		bool full_load_mode = false;
		bool run_stream_in_editor = true;
	};

	// Each LOD works in a set of coordinates spanning 2x more voxels the higher their index is
	struct Lod {
		// Keeping track of asynchronously loading blocks so we don't try to redundantly load them
		std::unordered_set<Vector3i> loading_blocks;
		BinaryMutex loading_blocks_mutex;

		// These are relative to this LOD, in block coordinates
		Vector3i last_viewer_data_block_pos;
		int last_view_distance_data_blocks = 0;

		VoxelMeshMap mesh_map;
		// Locked for writing when blocks get inserted or removed from the map.
		// If you need to lock more than one Lod, always do so in increasing order, to avoid deadlocks.
		// IMPORTANT:
		// - The update task only adds blocks to the map, and doesn't remove them
		// - Threads outside the update task must never add or remove blocks to the map (even with locking),
		//   unless the task has finished running
		RWLock mesh_map_lock;

		std::vector<Vector3i> blocks_pending_update;
		Vector3i last_viewer_mesh_block_pos;
		int last_view_distance_mesh_blocks = 0;

		// Deferred outputs to main thread
		std::vector<Vector3i> mesh_blocks_to_unload;
		std::vector<TransitionUpdate> mesh_blocks_to_update_transitions;

		inline bool has_loading_block(const Vector3i &pos) const {
			return loading_blocks.find(pos) != loading_blocks.end();
		}
	};

	struct AsyncEdit {
		IThreadedTask *task;
		Box3i box;
		std::shared_ptr<AsyncDependencyTracker> task_tracker;
	};

	struct RunningAsyncEdit {
		std::shared_ptr<AsyncDependencyTracker> tracker;
		Box3i box;
	};

	// Data modified by the update task
	struct State {
		// This terrain type is a sparse grid of octrees.
		// Indexed by a grid coordinate whose step is the size of the highest-LOD block.
		// Not using a pointer because Map storage is stable.
		// TODO Optimization: could be replaced with a grid data structure
		Map<Vector3i, OctreeItem> lod_octrees;
		Box3i last_octree_region_box;

		FixedArray<Lod, constants::MAX_LOD> lods;

		// This is the entry point for notifying data changes, which will cause mesh updates.
		// Contains blocks that were edited and need their LOD counterparts to be updated.
		// Scheduling is only done at LOD0 because it is the only editable LOD.
		std::vector<Vector3i> blocks_pending_lodding_lod0;
		BinaryMutex blocks_pending_lodding_lod0_mutex;

		std::vector<AsyncEdit> pending_async_edits;
		BinaryMutex pending_async_edits_mutex;
		std::vector<RunningAsyncEdit> running_async_edits;

		// Deferred outputs to main thread
		std::vector<VoxelMeshBlock *> mesh_blocks_to_activate;
		std::vector<VoxelMeshBlock *> mesh_blocks_to_deactivate;
	};

	// Set to true when the update task is finished
	std::atomic_bool task_is_complete;
	// Will be locked as long as the update task is running.
	BinaryMutex completion_mutex;

	Settings settings;
	State state;

	// After this call, no locking is necessary, as no other thread should be using the data.
	// However it can stall for longer, so prefer using it when doing structural changes, such as changing LOD count,
	// LOD distances, or the way the update logic runs.
	void wait_for_end_of_task() {
		MutexLock lock(completion_mutex);
	}
};

} // namespace voxel
} // namespace zylann

#endif // VOXEL_LOD_TERRAIN_UPDATE_DATA_H
