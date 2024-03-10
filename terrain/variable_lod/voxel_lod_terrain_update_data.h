#ifndef VOXEL_LOD_TERRAIN_UPDATE_DATA_H
#define VOXEL_LOD_TERRAIN_UPDATE_DATA_H

#include "../../constants/voxel_constants.h"
#include "../../engine/detail_rendering/detail_rendering.h"
#include "../../generators/voxel_generator.h"
#include "../../streams/voxel_stream.h"
#include "../../util/containers/fixed_array.h"
#include "../../util/containers/std_map.h"
#include "../../util/containers/std_unordered_map.h"
#include "../../util/containers/std_vector.h"
#include "../../util/safe_ref_count.h"
#include "../../util/tasks/cancellation_token.h"
#include "../voxel_mesh_map.h"
#include "lod_octree.h"

namespace zylann {

class AsyncDependencyTracker;

namespace voxel {

// Settings and states needed for the multi-threaded part of the update loop of VoxelLodTerrain.
// See `VoxelLodTerrainUpdateTask` for more info.
struct VoxelLodTerrainUpdateData {
	struct TransitionUpdate {
		Vector3i block_position;
		uint8_t transition_mask;
	};

	struct BlockLocation {
		Vector3i position;
		uint8_t lod;

		inline bool operator==(BlockLocation other) const {
			return position == other.position && other.lod == lod;
		}
	};

	struct BlockToLoad {
		BlockLocation loc;
		TaskCancellationToken cancellation_token;
	};

	// struct BlockToSave {
	// 	std::shared_ptr<VoxelBuffer> voxels;
	// 	Vector3i position;
	// 	uint8_t lod;
	// };

	enum StreamingSystem : uint8_t { //
		STREAMING_SYSTEM_LEGACY_OCTREE = 0,
		STREAMING_SYSTEM_CLIPBOX
	};

	// These values don't change during the update task.
	struct Settings {
		// Area within which voxels can exist.
		// Note, these bounds might not be exactly represented. This volume is chunk-based, so the result will be
		// approximated to the closest chunk.
		// Box3i bounds_in_voxels;
		// unsigned int lod_count = 0;

		// Distance between a viewer and the end of LOD0. May not be respected exactly, it can be rounded up
		float lod_distance = 0.f;
		// Distance between the end of LOD0 and the end of LOD1, carried over to other LODs
		float secondary_lod_distance = 0.f;
		unsigned int view_distance_voxels = 512;
		StreamingSystem streaming_system = STREAMING_SYSTEM_LEGACY_OCTREE;
		// bool full_load_mode = false;
		bool run_stream_in_editor = true;
		// If true, try to generate blocks and store them in the data map before posting mesh requests.
		// If false, meshing will generate non-edited voxels on the fly instead.
		// Not really exposed for now, will wait for it to be really needed. It might never be.
		bool cache_generated_blocks = false;
		bool collision_enabled = true;
		bool detail_textures_use_gpu = false;
		bool generator_use_gpu = false;
		uint8_t detail_texture_generator_override_begin_lod_index = 0;
		unsigned int mesh_block_size_po2 = 4;
		DetailRenderingSettings detail_texture_settings;
		Ref<VoxelGenerator> detail_texture_generator_override;
	};

	enum MeshState {
		MESH_NEVER_UPDATED = 0, // TODO Redundant with MESH_NEED_UPDATE?
		MESH_UP_TO_DATE,
		MESH_NEED_UPDATE, // The mesh is out of date but was not yet scheduled for update
		MESH_UPDATE_NOT_SENT, // The mesh is out of date and was scheduled for update, but no request have been sent
							  // yet
		MESH_UPDATE_SENT // The mesh is out of date, and an update request was sent, pending response
	};

	enum DetailTextureState { //
		DETAIL_TEXTURE_IDLE = 0,
		DETAIL_TEXTURE_NEED_UPDATE,
		DETAIL_TEXTURE_PENDING
	};

	struct MeshBlockState {
		std::atomic<MeshState> state;
		std::atomic<DetailTextureState> detail_texture_state;

		// Refcount here to support multiple viewers, we can't do it on the main thread's mesh map since the
		// streaming logic is in the update task.
		// TODO Optimize: this could almost not need to be atomic.
		// This is an atomic refcount only because the main thread needs to read it when receiving mesh updates. It
		// could be made non-atomic if mesh updates were handled in the threaded update, but that has a more
		// implications than making this atomic for now.
		SafeRefCount mesh_viewers;
		SafeRefCount collision_viewers;

		// Cancelled when this mesh block is removed, so if tasks are still queued to do work for that block, they
		// will be cancelled
		TaskCancellationToken cancellation_token;

		// Index within the list of meshes to update during one update of the terrain. Used to avoid putting the same
		// mesh more than once in the list, while allowing to change options after it's been added to the list. Should
		// reset to -1 after each update (since the list is consumed)
		int update_list_index;

		uint8_t transition_mask;
		bool visual_active;
		bool collision_active;

		// Tells whether the first meshing was done since this block was added.
		// Written by the main thread only, when it receives mesh updates or when it unloads resources.
		// Read by threaded update to decide when to subdivide LODs.
		std::atomic_bool visual_loaded;
		std::atomic_bool collision_loaded;

		// bool pending_update_has_visuals;
		// bool pending_update_has_collision;

		MeshBlockState() :
				state(MESH_NEVER_UPDATED),
				detail_texture_state(DETAIL_TEXTURE_IDLE),
				update_list_index(-1),
				transition_mask(0),
				visual_active(false),
				collision_active(false),
				visual_loaded(false),
				collision_loaded(false) {}
	};

	// Version of the mesh map designed to be mainly used for the threaded update task.
	// It contains states used to determine when to actually load/unload meshes.
	struct MeshMapState {
		// Values in this map are expected to have stable addresses.
		StdUnorderedMap<Vector3i, MeshBlockState> map;
		// Locked for writing when blocks get inserted or removed from the map.
		// If you need to lock more than one Lod, always do so in increasing order, to avoid deadlocks.
		// IMPORTANT:
		// - Only the update task will add and remove blocks from this map.
		// - Threads outside the update task must never add or remove blocks to the map (even with locking),
		//   unless the task is not running in parallel.
		// - Threads outside the update task must always lock it, unless the update task isn't running.
		// - The update task doesn't need to lock it, unless when adding or removing blocks.
		RWLock map_lock;
	};

	struct LoadingDataBlock {
		RefCount viewers;
		TaskCancellationToken cancellation_token;
	};

	struct MeshToUpdate {
		Vector3i position;
		TaskCancellationToken cancellation_token;
		bool require_visual = false;
	};

	struct QuickReloadingBlock {
		std::shared_ptr<VoxelBuffer> voxels;
		Vector3i position;
	};

	// Each LOD works in a set of coordinates spanning 2x more voxels the higher their index is
	struct Lod {
		// Keeping track of asynchronously loading blocks so we don't try to redundantly load them
		StdUnorderedMap<Vector3i, LoadingDataBlock> loading_blocks;
		BinaryMutex loading_blocks_mutex;

		// Blocks waiting to be saved after they got unloaded. This is to allow reloading them properly if a viewer
		// needs them again before they even got saved. Items in this cache get removed when they are saved. Needs to be
		// protected by mutex because the saved notification is received on the main thread at the moment.
		StdUnorderedMap<Vector3i, std::shared_ptr<VoxelBuffer>> unloaded_saving_blocks;
		BinaryMutex unloaded_saving_blocks_mutex;
		// Blocks that will be loaded from the saving cache as if a loading task completed next time the terrain
		// updates. It won't run while the threaded update runs so no locking is needed.
		StdVector<QuickReloadingBlock> quick_reloading_blocks;

		// These are relative to this LOD, in block coordinates
		Vector3i last_viewer_data_block_pos;
		int last_view_distance_data_blocks = 0;

		MeshMapState mesh_map_state;

		// Positions of mesh blocks that will be scheduled for update next time the update task runs.
		StdVector<MeshToUpdate> mesh_blocks_pending_update;
		Vector3i last_viewer_mesh_block_pos;
		int last_view_distance_mesh_blocks = 0;

		// Deferred outputs to main thread. Should only be read once the task is finished, so no need to lock.
		// TODO These output actions are not particularly serialized, that might cause issues (havent so far).
		StdVector<Vector3i> mesh_blocks_to_unload;
		StdVector<TransitionUpdate> mesh_blocks_to_update_transitions;
		StdVector<Vector3i> mesh_blocks_to_activate_visuals;
		StdVector<Vector3i> mesh_blocks_to_deactivate_visuals;
		StdVector<Vector3i> mesh_blocks_to_activate_collision;
		StdVector<Vector3i> mesh_blocks_to_deactivate_collision;
		StdVector<Vector3i> mesh_blocks_to_drop_visual;
		StdVector<Vector3i> mesh_blocks_to_drop_collision;

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

	struct Stats {
		uint32_t blocked_lods = 0;
		uint32_t time_detect_required_blocks = 0;
		uint32_t time_io_requests = 0;
		uint32_t time_mesh_requests = 0;
		uint32_t time_total = 0;
	};

	struct OctreeItem {
		LodOctree octree;
	};

	struct OctreeStreamingState {
		// This terrain type is a sparse grid of octrees.
		// Indexed by a grid coordinate whose step is the size of the highest-LOD block.
		// Not using a pointer because Map storage is stable.
		// TODO Optimization: could be replaced with a grid data structure
		StdMap<Vector3i, OctreeItem> lod_octrees;
		Box3i last_octree_region_box;
		Vector3i local_viewer_pos_previous_octree_update;

		// Tells if there were nodes that needed to split or merge but could not due to pending dependencies.
		// This affects whether octree streaming will need to be processed again on the next update.
		bool had_blocked_octree_nodes_previous_update = false;

		bool force_update_octrees_next_update = false;
	};

	// Paired viewers are VoxelViewers which intersect with the boundaries of the volume
	struct PairedViewer {
		struct State {
			Vector3i local_position_voxels;

			// In block coordinates
			FixedArray<Box3i, constants::MAX_LOD> data_box_per_lod;
			FixedArray<Box3i, constants::MAX_LOD> mesh_box_per_lod;

			int view_distance_voxels = 0;
			bool requires_collisions = false;
			bool requires_visuals = false;
		};
		ViewerID id;
		State state;
		State prev_state;
	};

	struct LoadedMeshBlockEvent {
		Vector3i position;
		uint8_t lod_index;
		bool visual;
		bool collision;
	};

	struct ClipboxStreamingState {
		StdVector<PairedViewer> paired_viewers;
		// Vector3i viewer_pos_in_lod0_voxels_previous_update;
		// int lod_distance_in_data_chunks_previous_update = 0;
		// int lod_distance_in_mesh_chunks_previous_update = 0;

		// Written by main thread when data blocks are received.
		// Read by update thread to trigger meshing.
		StdVector<BlockLocation> loaded_data_blocks;
		BinaryMutex loaded_data_blocks_mutex;

		// Written by main thread when mesh blocks are received (and there was previously no mesh).
		// Read by update thread to trigger visibility changes.
		StdVector<LoadedMeshBlockEvent> loaded_mesh_blocks;
		BinaryMutex loaded_mesh_blocks_mutex;
	};

	struct EditNotificationInputs {
		// Entry point for notifying data changes, which will cause data LODs and mesh updates.
		// Contains blocks that were edited and need their LOD counterparts to be updated.
		// Scheduling is only done at LOD0 because it is the only editable LOD.

		// Used specifically for lodding voxels
		StdVector<Vector3i> edited_blocks_lod0;
		// Used specifically to update meshes
		// TODO Maybe we could use only that? The reason we have edited blocks separately is because edits might affect
		// only specific blocks and not the full area
		StdVector<Box3i> edited_voxel_areas_lod0;

		BinaryMutex mutex;
	};

	// Data modified by the update task
	struct State {
		OctreeStreamingState octree_streaming;
		ClipboxStreamingState clipbox_streaming;

		FixedArray<Lod, constants::MAX_LOD> lods;

		EditNotificationInputs edit_notifications;

		StdVector<AsyncEdit> pending_async_edits;
		BinaryMutex pending_async_edits_mutex;
		StdVector<RunningAsyncEdit> running_async_edits;

		// Areas where generated stuff has changed. Similar to an edit, but non-destructive.
		StdVector<Box3i> changed_generated_areas;
		BinaryMutex changed_generated_areas_mutex;

		Stats stats;
	};

	// Set to true when the update task is finished
	std::atomic_bool task_is_complete = { true };
	// Will be locked as long as the update task is running.
	BinaryMutex completion_mutex;

	Settings settings;
	State state;

	// Copy of all viewers, since accessing them directly in VoxelEngine is not thread safe at the moment
	StdVector<std::pair<ViewerID, VoxelEngine::Viewer>> viewers;

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
