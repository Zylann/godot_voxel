#ifndef VOXEL_LOD_TERRAIN_UPDATE_TASK_H
#define VOXEL_LOD_TERRAIN_UPDATE_TASK_H

#include "../../engine/ids.h"
#include "../../engine/priority_dependency.h"
#include "../../util/tasks/threaded_task.h"
#include "voxel_lod_terrain_update_data.h"

namespace zylann::voxel {

struct StreamingDependency;
struct MeshingDependency;

// Runs a part of the update loop of a VoxelLodTerrain.
// This part can run on another thread, so multiple terrains can update in parallel.
// There must be only one running at once per terrain.
// Note, this task does not include meshing and voxel generation. These are done with different tasks.
//
// IMPORTANT: The work done by this task must not involve any call to Godot's servers, directly or indirectly.
// These are deferred to the main thread.
//
class VoxelLodTerrainUpdateTask : public IThreadedTask {
public:
	VoxelLodTerrainUpdateTask(std::shared_ptr<VoxelData> p_data,
			std::shared_ptr<VoxelLodTerrainUpdateData> p_update_data,
			std::shared_ptr<StreamingDependency> p_streaming_dependency,
			std::shared_ptr<MeshingDependency> p_meshing_dependency,
			std::shared_ptr<PriorityDependency::ViewersData> p_shared_viewers_data, Vector3 p_viewer_pos,
			bool p_request_instances, VolumeID p_volume_id, Transform3D p_volume_transform) :
			//
			_data(p_data),
			_update_data(p_update_data),
			_streaming_dependency(p_streaming_dependency),
			_meshing_dependency(p_meshing_dependency),
			_shared_viewers_data(p_shared_viewers_data),
			_viewer_pos(p_viewer_pos),
			_request_instances(p_request_instances),
			_volume_id(p_volume_id),
			_volume_transform(p_volume_transform) {}

	const char *get_debug_name() const override {
		return "VoxelLodTerrainUpdate";
	}

	void run(ThreadedTaskContext ctx) override;

	// Functions also used outside of this task

	static void flush_pending_lod_edits(
			VoxelLodTerrainUpdateData::State &state, VoxelData &data, const int mesh_block_size);

	static uint8_t get_transition_mask(const VoxelLodTerrainUpdateData::State &state, Vector3i block_pos,
			unsigned int lod_index, unsigned int lod_count);

	// To use on loaded blocks
	static inline void schedule_mesh_update(VoxelLodTerrainUpdateData::MeshBlockState &block, Vector3i bpos,
			std::vector<Vector3i> &blocks_pending_update) {
		if (block.state != VoxelLodTerrainUpdateData::MESH_UPDATE_NOT_SENT) {
			if (block.active) {
				// Schedule an update
				block.state = VoxelLodTerrainUpdateData::MESH_UPDATE_NOT_SENT;
				blocks_pending_update.push_back(bpos);
			} else {
				// Just mark it as needing update, so the visibility system will schedule its update when needed
				block.state = VoxelLodTerrainUpdateData::MESH_NEED_UPDATE;
			}
		}
	}

	static void send_block_save_requests(VolumeID volume_id, Span<VoxelData::BlockToSave> blocks_to_save,
			std::shared_ptr<StreamingDependency> &stream_dependency, unsigned int data_block_size,
			BufferedTaskScheduler &task_scheduler);

private:
	std::shared_ptr<VoxelData> _data;
	std::shared_ptr<VoxelLodTerrainUpdateData> _update_data;
	std::shared_ptr<StreamingDependency> _streaming_dependency;
	std::shared_ptr<MeshingDependency> _meshing_dependency;
	std::shared_ptr<PriorityDependency::ViewersData> _shared_viewers_data;
	Vector3 _viewer_pos;
	bool _request_instances;
	VolumeID _volume_id;
	Transform3D _volume_transform;
};

} // namespace zylann::voxel

#endif // VOXEL_LOD_TERRAIN_UPDATE_TASK_H
