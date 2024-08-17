#include "voxel_lod_terrain_update_task.h"
#include "../../engine/buffered_task_scheduler.h"
#include "../../engine/voxel_engine.h"
#include "../../generators/generate_block_task.h"
#include "../../meshers/mesh_block_task.h"
#include "../../meshers/transvoxel/voxel_mesher_transvoxel.h"
#include "../../storage/voxel_data.h"
#include "../../streams/load_block_data_task.h"
#include "../../streams/save_block_data_task.h"
#include "../../util/containers/container_funcs.h"
#include "../../util/dstack.h"
#include "../../util/godot/classes/engine.h"
#include "../../util/math/conv.h"
#include "../../util/profiling.h"
#include "../../util/profiling_clock.h"
#include "../../util/string/format.h"
#include "../../util/tasks/async_dependency_tracker.h"
#include "voxel_lod_terrain_update_clipbox_streaming.h"
#include "voxel_lod_terrain_update_octree_streaming.h"

namespace zylann::voxel {

namespace {

inline Vector3i get_block_center(Vector3i pos, int bs, int lod) {
	return (pos << lod) * bs + Vector3iUtil::create(bs / 2);
}

void init_sparse_octree_priority_dependency( //
		PriorityDependency &dep, //
		Vector3i block_position, //
		uint8_t lod, //
		int data_block_size, //
		std::shared_ptr<PriorityDependency::ViewersData> &shared_viewers_data, //
		const Transform3D &volume_transform, //
		float octree_lod_distance //
) {
	//
	const Vector3i voxel_pos = get_block_center(block_position, data_block_size, lod);
	const float block_radius = (data_block_size << lod) / 2;
	dep.shared = shared_viewers_data;
	dep.world_position = to_vec3f(volume_transform.xform(voxel_pos));
	const float transformed_block_radius =
			volume_transform.basis.xform(Vector3(block_radius, block_radius, block_radius)).length();

	// Distance beyond which it is safe to drop a block without risking to block LOD subdivision.
	// This does not depend on viewer's view distance, but on LOD precision instead.
	// TODO Should `data_block_size` be used here? Should it be mesh_block_size instead?
	dep.drop_distance_squared = math::squared(2.f * transformed_block_radius *
			VoxelEngine::get_octree_lod_block_region_extent(octree_lod_distance, data_block_size));
}

// This is only if we want to cache voxel data
void request_block_generate( //
		VolumeID volume_id, //
		unsigned int data_block_size, //
		std::shared_ptr<StreamingDependency> &stream_dependency, //
		const std::shared_ptr<VoxelData> &data, //
		Vector3i block_pos, //
		int lod_index, //
		std::shared_ptr<PriorityDependency::ViewersData> &shared_viewers_data, //
		const Transform3D &volume_transform, //
		const VoxelLodTerrainUpdateData::Settings &settings, //
		std::shared_ptr<AsyncDependencyTracker> tracker, //
		bool allow_drop, //
		BufferedTaskScheduler &task_scheduler, //
		TaskCancellationToken cancellation_token //
) {
	//
	CRASH_COND(data_block_size > 255);
	CRASH_COND(stream_dependency == nullptr);

	// We should not have done this request in the first place if both stream and generator are null
	ERR_FAIL_COND(stream_dependency->generator.is_null());

	VoxelGenerator::BlockTaskParams params;
	params.volume_id = volume_id;
	params.block_position = block_pos;
	params.lod_index = lod_index;
	params.block_size = data_block_size;
	params.stream_dependency = stream_dependency;
	params.tracker = tracker;
	params.drop_beyond_max_distance = allow_drop;
	params.data = data;
	params.use_gpu = settings.generator_use_gpu;
	params.cancellation_token = cancellation_token;

	init_sparse_octree_priority_dependency(params.priority_dependency, block_pos, lod_index, data_block_size,
			shared_viewers_data, volume_transform, settings.lod_distance);

	IThreadedTask *task = stream_dependency->generator->create_block_task(params);

	task_scheduler.push_main_task(task);
}

// Used only when streaming block by block
void request_block_load( //
		VolumeID volume_id, //
		unsigned int data_block_size, //
		std::shared_ptr<StreamingDependency> &stream_dependency, //
		const std::shared_ptr<VoxelData> &data, //
		Vector3i block_pos, //
		int lod_index, //
		std::shared_ptr<PriorityDependency::ViewersData> &shared_viewers_data, //
		const Transform3D &volume_transform, //
		const VoxelLodTerrainUpdateData::Settings &settings, //
		BufferedTaskScheduler &task_scheduler, //
		TaskCancellationToken cancellation_token, //
		VoxelLodTerrainUpdateData::State &state //
) {
	//
	ZN_ASSERT(data_block_size < 256);
	ZN_ASSERT(stream_dependency != nullptr);

	VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];
	{
		std::shared_ptr<VoxelBuffer> voxels;
		{
			MutexLock mlock(lod.unloaded_saving_blocks_mutex);
			auto saving_it = lod.unloaded_saving_blocks.find(block_pos);
			if (saving_it != lod.unloaded_saving_blocks.end()) {
				voxels = saving_it->second;
			}
		}
		if (voxels != nullptr) {
			lod.quick_reloading_blocks.push_back(VoxelLodTerrainUpdateData::QuickReloadingBlock{ voxels, block_pos });
			return;
		}
	}

	if (stream_dependency->stream.is_valid()) {
		PriorityDependency priority_dependency;
		init_sparse_octree_priority_dependency(priority_dependency, block_pos, lod_index, data_block_size,
				shared_viewers_data, volume_transform, settings.lod_distance);

		const bool request_instances = false;
		LoadBlockDataTask *task = ZN_NEW(LoadBlockDataTask(volume_id, block_pos, lod_index, data_block_size,
				request_instances, stream_dependency, priority_dependency, settings.cache_generated_blocks,
				settings.generator_use_gpu, data, cancellation_token));

		task_scheduler.push_io_task(task);

	} else if (settings.cache_generated_blocks) {
		// Directly generate the block without checking the stream.
		request_block_generate(volume_id, data_block_size, stream_dependency, data, block_pos, lod_index,
				shared_viewers_data, volume_transform, settings, nullptr, true, task_scheduler, cancellation_token);

	} else {
		ZN_PRINT_WARNING("Requesting a block load when it should not have been necessary");
	}
}

void send_block_data_requests( //
		VolumeID volume_id, //
		Span<const VoxelLodTerrainUpdateData::BlockToLoad> blocks_to_load, //
		std::shared_ptr<StreamingDependency> &stream_dependency, //
		const std::shared_ptr<VoxelData> &data, //
		std::shared_ptr<PriorityDependency::ViewersData> &shared_viewers_data, //
		unsigned int data_block_size, //
		const Transform3D &volume_transform, //
		const VoxelLodTerrainUpdateData::Settings &settings, //
		BufferedTaskScheduler &task_scheduler, //
		VoxelLodTerrainUpdateData::State &state //
) {
	//
	for (unsigned int i = 0; i < blocks_to_load.size(); ++i) {
		const VoxelLodTerrainUpdateData::BlockToLoad btl = blocks_to_load[i];
		request_block_load( //
				volume_id, //
				data_block_size, //
				stream_dependency, //
				data, //
				btl.loc.position, //
				btl.loc.lod, //
				shared_viewers_data, //
				volume_transform, //
				settings, //
				task_scheduler, //
				btl.cancellation_token, //
				state //
		);
	}
}

// This is used when streaming is enabled, yet the terrain has no stream and no generator (There can only be empty
// blocks when moving around), or generating is configured to happen on the fly during meshing.
// So we have to simulate a VoxelStream that returns empty blocks immediately.
void apply_block_data_requests_as_empty( //
		Span<const VoxelLodTerrainUpdateData::BlockToLoad> blocks_to_load, //
		VoxelData &data, //
		VoxelLodTerrainUpdateData::State &state, //
		const VoxelLodTerrainUpdateData::Settings &settings //
) {
	ZN_PROFILE_SCOPE();
	ZN_ASSERT_RETURN(data.is_streaming_enabled());

	for (const VoxelLodTerrainUpdateData::BlockToLoad &btl : blocks_to_load) {
		VoxelLodTerrainUpdateData::Lod &lod = state.lods[btl.loc.lod];
		RefCount viewers;
		{
			MutexLock mlock(lod.loading_blocks_mutex);
			auto it = lod.loading_blocks.find(btl.loc.position);
			if (it != lod.loading_blocks.end()) {
				viewers = it->second.viewers;
				lod.loading_blocks.erase(it);
			} else {
				ZN_PRINT_ERROR("Loading block wasn't found when consuming data requests as empty");
			}
		}
		{
			// The block is considered "loaded" as we know there is nothing to load from a save file,
			// and generating can be done on the fly if present, so this is represented by assigning a data block with
			// no voxels attached.
			VoxelDataBlock empty_block(btl.loc.lod);
			empty_block.viewers = viewers;
			data.try_set_block(btl.loc.position, empty_block);
		}
	}

	if (settings.streaming_system == VoxelLodTerrainUpdateData::STREAMING_SYSTEM_CLIPBOX) {
		// Since streaming is enabled, the system must be told this block is now "loaded", because it doesn't use
		// polling to know when things are loaded
		MutexLock mlock(state.clipbox_streaming.loaded_data_blocks_mutex);
		for (const VoxelLodTerrainUpdateData::BlockToLoad &btl : blocks_to_load) {
			state.clipbox_streaming.loaded_data_blocks.push_back(btl.loc);
		}
	}
}

void request_voxel_block_save( //
		VolumeID volume_id, //
		std::shared_ptr<VoxelBuffer> &voxels, //
		Vector3i block_pos, //
		int lod_index, //
		std::shared_ptr<StreamingDependency> &stream_dependency, //
		BufferedTaskScheduler &task_scheduler, //
		std::shared_ptr<AsyncDependencyTracker> tracker, //
		bool with_flush //
) {
	CRASH_COND(stream_dependency == nullptr);
	ERR_FAIL_COND(stream_dependency->stream.is_null());

	SaveBlockDataTask *task =
			ZN_NEW(SaveBlockDataTask(volume_id, block_pos, lod_index, voxels, stream_dependency, tracker, with_flush));

	// No priority data, saving doesn't need sorting.

	task_scheduler.push_io_task(task);
}

void send_mesh_requests( //
		VolumeID volume_id, //
		VoxelLodTerrainUpdateData::State &state, //
		const VoxelLodTerrainUpdateData::Settings &settings, //
		const std::shared_ptr<VoxelData> &data_ptr, //
		std::shared_ptr<MeshingDependency> meshing_dependency, //
		std::shared_ptr<PriorityDependency::ViewersData> &shared_viewers_data, //
		const Transform3D &volume_transform, //
		BufferedTaskScheduler &task_scheduler //
) {
	ZN_PROFILE_SCOPE();

	ZN_ASSERT(data_ptr != nullptr);
	const VoxelData &data = *data_ptr;

	const int data_block_size = data.get_block_size();
	const int mesh_block_size = 1 << settings.mesh_block_size_po2;
	const int render_to_data_factor = mesh_block_size / data_block_size;
	const unsigned int lod_count = data.get_lod_count();

	for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
		ZN_PROFILE_SCOPE();
		VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];

		for (unsigned int bi = 0; bi < lod.mesh_blocks_pending_update.size(); ++bi) {
			ZN_PROFILE_SCOPE();
			const VoxelLodTerrainUpdateData::MeshToUpdate &mesh_to_update = lod.mesh_blocks_pending_update[bi];

			auto mesh_block_it = lod.mesh_map_state.map.find(mesh_to_update.position);
			// A block must have been allocated before we ask for a mesh update
			ZN_ASSERT_CONTINUE(mesh_block_it != lod.mesh_map_state.map.end());
			VoxelLodTerrainUpdateData::MeshBlockState &mesh_block = mesh_block_it->second;
			// All blocks we get here must be in the scheduled state
			ZN_ASSERT_CONTINUE(mesh_block.state == VoxelLodTerrainUpdateData::MESH_UPDATE_NOT_SENT);

			// Get block and its neighbors
			// VoxelEngine::BlockMeshInput mesh_request;
			// mesh_request.render_block_position = mesh_block_pos;
			// mesh_request.lod = lod_index;

			// We'll allocate this quite often. If it becomes a problem, it should be easy to pool.
			MeshBlockTask *task = ZN_NEW(MeshBlockTask);
			task->volume_id = volume_id;
			task->mesh_block_position = mesh_to_update.position;
			task->lod_index = lod_index;
			task->lod_hint = true;
			task->meshing_dependency = meshing_dependency;
			task->data = data_ptr;
			task->require_visual = mesh_to_update.require_visual;
			task->collision_hint = settings.collision_enabled;
			task->detail_texture_settings = settings.detail_texture_settings;
			task->detail_texture_generator_override = settings.detail_texture_generator_override;
			task->detail_texture_generator_override_begin_lod_index =
					settings.detail_texture_generator_override_begin_lod_index;
			task->detail_texture_use_gpu = settings.detail_textures_use_gpu;
			task->block_generation_use_gpu = settings.generator_use_gpu;
			task->cancellation_token = mesh_to_update.cancellation_token;

			// Don't update a detail texture if one update is already processing
			if (settings.detail_texture_settings.enabled &&
					lod_index >= settings.detail_texture_settings.begin_lod_index &&
					mesh_block.detail_texture_state != VoxelLodTerrainUpdateData::DETAIL_TEXTURE_PENDING) {
				mesh_block.detail_texture_state = VoxelLodTerrainUpdateData::DETAIL_TEXTURE_PENDING;
				task->require_detail_texture = true;
			}

			const Box3i data_box =
					Box3i(render_to_data_factor * mesh_to_update.position, Vector3iUtil::create(render_to_data_factor))
							.padded(1);

			// Iteration order matters for thread access.
			// The array also implicitly encodes block position due to the convention being used,
			// so there is no need to also include positions in the request
			data.get_blocks_with_voxel_data(data_box, lod_index, to_span(task->blocks));
			task->blocks_count = Vector3iUtil::get_volume(data_box.size);

			// TODO There is inconsistency with coordinates sent to this function.
			// Sometimes we send data block coordinates, sometimes we send mesh block coordinates. They aren't always
			// the same, it might cause issues in priority sorting?
			init_sparse_octree_priority_dependency(task->priority_dependency, task->mesh_block_position,
					task->lod_index, mesh_block_size, shared_viewers_data, volume_transform, settings.lod_distance);

			task_scheduler.push_main_task(task);

			mesh_block.state = VoxelLodTerrainUpdateData::MESH_UPDATE_SENT;
			mesh_block.update_list_index = -1;
		}

		lod.mesh_blocks_pending_update.clear();
	}
}

// Generates all non-present blocks in preparation for an edit.
// This function schedules one parallel task for every block.
// The returned tracker may be polled to detect when it is complete.
// Only used in full load mode, because in streaming mode blocks must be present already.
std::shared_ptr<AsyncDependencyTracker> preload_boxes_async( //
		VoxelLodTerrainUpdateData::State &state, //
		const VoxelLodTerrainUpdateData::Settings &settings, //
		const std::shared_ptr<VoxelData> data_ptr, //
		Span<const Box3i> voxel_boxes, //
		Span<IThreadedTask *> next_tasks, //
		VolumeID volume_id, //
		std::shared_ptr<StreamingDependency> &stream_dependency, //
		std::shared_ptr<PriorityDependency::ViewersData> &shared_viewers_data, //
		const Transform3D &volume_transform, //
		BufferedTaskScheduler &task_scheduler //
) {
	ZN_PROFILE_SCOPE();

	ZN_ASSERT(data_ptr != nullptr);
	VoxelData &data = *data_ptr;

	ZN_ASSERT_RETURN_V_MSG(
			data.is_streaming_enabled() == false, nullptr, "This function can only be used in full load mode");

	struct TaskArguments {
		Vector3i block_pos;
		unsigned int lod_index;
	};

	StdVector<TaskArguments> todo;

	const unsigned int data_block_size = data.get_block_size();
	const unsigned int lod_count = data.get_lod_count();

	for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
		for (unsigned int box_index = 0; box_index < voxel_boxes.size(); ++box_index) {
			ZN_PROFILE_SCOPE_NAMED("Box");

			VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];
			const Box3i voxel_box = voxel_boxes[box_index];
			const Box3i block_box = voxel_box.downscaled(data_block_size << lod_index);

			// ZN_PRINT_VERBOSE(String("Preloading box {0} at lod {1}")
			// 						.format(varray(block_box.to_string(), lod_index)));

			static thread_local StdVector<Vector3i> tls_missing;
			tls_missing.clear();
			data.get_missing_blocks(block_box, lod_index, tls_missing);

			if (tls_missing.size() > 0) {
				// MutexLock mlock(lod.loading_blocks_mutex);
				for (const Vector3i &missing_bpos : tls_missing) {
					if (!lod.has_loading_block(missing_bpos)) {
						todo.push_back(TaskArguments{ missing_bpos, lod_index });
						// We should not need to populate loading_blocks in full load mode
						// lod.loading_blocks.insert(missing_bpos);
					}
				}
			}
		}
	}

	ZN_PRINT_VERBOSE(format("Preloading boxes with {} tasks", todo.size()));

	std::shared_ptr<AsyncDependencyTracker> tracker = nullptr;

	// TODO `next_tasks` is executed in parallel. But since they can be edits, may we do them in sequence?

	if (todo.size() > 0) {
		ZN_PROFILE_SCOPE_NAMED("Posting requests");

		// Only create the tracker if we actually are creating tasks. If we still create it,
		// no task will take ownership of it, so if it is not stored after this function returns,
		// it would destroy `next_tasks`.

		// This may first run the generation tasks, and then the edits
		tracker = make_shared_instance<AsyncDependencyTracker>(
				todo.size(), next_tasks, [](Span<IThreadedTask *> p_next_tasks) {
					VoxelEngine::get_singleton().push_async_tasks(p_next_tasks);
				});

		for (unsigned int i = 0; i < todo.size(); ++i) {
			const TaskArguments args = todo[i];
			request_block_generate( //
					volume_id, //
					data_block_size, //
					stream_dependency, //
					data_ptr, //
					args.block_pos, //
					args.lod_index, //
					shared_viewers_data, //
					volume_transform, //
					settings, //
					tracker, //
					false, //
					task_scheduler, //
					TaskCancellationToken() //
			);
		}

	} else if (next_tasks.size() > 0) {
		// Nothing to preload, we may schedule `next_tasks` right now
		VoxelEngine::get_singleton().push_async_tasks(next_tasks);
	}

	return tracker;
}

void process_async_edits( //
		VoxelLodTerrainUpdateData::State &state, //
		const VoxelLodTerrainUpdateData::Settings &settings, //
		const std::shared_ptr<VoxelData> &data, //
		VolumeID volume_id, //
		std::shared_ptr<StreamingDependency> &stream_dependency, //
		std::shared_ptr<PriorityDependency::ViewersData> &shared_viewers_data, //
		const Transform3D &volume_transform, //
		BufferedTaskScheduler &task_scheduler //
) {
	ZN_PROFILE_SCOPE();

	if (state.running_async_edits.size() == 0) {
		// Schedule all next edits when the previous ones are done

		StdVector<Box3i> boxes_to_preload;
		StdVector<IThreadedTask *> tasks_to_schedule;
		std::shared_ptr<AsyncDependencyTracker> last_tracker = nullptr;

		for (unsigned int edit_index = 0; edit_index < state.pending_async_edits.size(); ++edit_index) {
			VoxelLodTerrainUpdateData::AsyncEdit &edit = state.pending_async_edits[edit_index];
			CRASH_COND(edit.task_tracker->has_next_tasks());

			// Not sure if worth doing, I don't think tasks can be aborted before even being scheduled.
			if (edit.task_tracker->is_aborted()) {
				ZN_PRINT_VERBOSE("Aborted async edit");
				ZN_DELETE(edit.task);
				continue;
			}

			boxes_to_preload.push_back(edit.box);
			tasks_to_schedule.push_back(edit.task);
			state.running_async_edits.push_back(
					VoxelLodTerrainUpdateData::RunningAsyncEdit{ edit.task_tracker, edit.box });
		}

		if (boxes_to_preload.size() > 0) {
			preload_boxes_async( //
					state, //
					settings, //
					data, //
					to_span_const(boxes_to_preload), //
					to_span(tasks_to_schedule), //
					volume_id, //
					stream_dependency, //
					shared_viewers_data, //
					volume_transform, //
					task_scheduler //
			);
		}

		state.pending_async_edits.clear();
	}
}

void process_changed_generated_areas( //
		VoxelLodTerrainUpdateData::State &state, //
		const VoxelLodTerrainUpdateData::Settings &settings, //
		unsigned int lod_count //
) {
	const unsigned int mesh_block_size = 1 << settings.mesh_block_size_po2;

	MutexLock lock(state.changed_generated_areas_mutex);
	if (state.changed_generated_areas.size() == 0) {
		return;
	}

	for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
		VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];

		for (auto box_it = state.changed_generated_areas.begin(); box_it != state.changed_generated_areas.end();
				++box_it) {
			const Box3i &voxel_box = *box_it;
			const Box3i bbox = voxel_box.padded(1).downscaled(mesh_block_size << lod_index);

			// TODO If there are cached generated blocks, they need to be re-cached or removed

			RWLockRead rlock(lod.mesh_map_state.map_lock);

			bbox.for_each_cell_zxy([&lod](const Vector3i bpos) {
				auto block_it = lod.mesh_map_state.map.find(bpos);
				if (block_it != lod.mesh_map_state.map.end()) {
					VoxelLodTerrainUpdateTask::schedule_mesh_update(block_it->second, bpos,
							lod.mesh_blocks_pending_update, block_it->second.mesh_viewers.get() > 0);
				}
			});
		}
	}

	state.changed_generated_areas.clear();
}

} // namespace

void VoxelLodTerrainUpdateTask::send_block_save_requests( //
		VolumeID volume_id, //
		Span<VoxelData::BlockToSave> blocks_to_save, //
		std::shared_ptr<StreamingDependency> &stream_dependency, //
		BufferedTaskScheduler &task_scheduler, //
		std::shared_ptr<AsyncDependencyTracker> tracker, //
		bool with_flush //
) {
	//
	for (unsigned int i = 0; i < blocks_to_save.size(); ++i) {
		VoxelData::BlockToSave &b = blocks_to_save[i];
		ZN_PRINT_VERBOSE(format("Requesting save of block {} lod {}", b.position, b.lod_index));
		request_voxel_block_save( //
				volume_id, //
				b.voxels, //
				b.position, //
				b.lod_index, //
				stream_dependency, //
				task_scheduler, //
				tracker, //
				with_flush //
		);
	}
}

void VoxelLodTerrainUpdateTask::flush_pending_lod_edits( //
		VoxelLodTerrainUpdateData::State &state, //
		VoxelData &data, //
		const int mesh_block_size //
) {
	ZN_DSTACK();
	ZN_PROFILE_SCOPE();

	static thread_local StdVector<Vector3i> tls_modified_lod0_blocks;
	static thread_local StdVector<Box3i> tls_modified_voxel_areas_lod0;
	// static thread_local StdVector<VoxelData::BlockLocation> tls_updated_block_locations;

	tls_modified_lod0_blocks.clear();
	tls_modified_voxel_areas_lod0.clear();

	// Consume inputs
	{
		MutexLock lock(state.edit_notifications.mutex);

		// Not sure if could just use `=`? What would std::vector do with capacity?
		append_array(tls_modified_lod0_blocks, state.edit_notifications.edited_blocks_lod0);
		append_array(tls_modified_voxel_areas_lod0, state.edit_notifications.edited_voxel_areas_lod0);

		state.edit_notifications.edited_blocks_lod0.clear();
		state.edit_notifications.edited_voxel_areas_lod0.clear();
	}

	// Update all data LODs
	// tls_updated_block_locations.clear();
	data.update_lods(to_span(tls_modified_lod0_blocks), nullptr);

	// Update affected meshes.
	// TODO Optimize: trigger mesh updates at LOD0 earlier? There is a bit of latency due to doing all the mipping work
	// first, and we know the edit happens before mipping anyways
	const unsigned int lod_count = data.get_lod_count();
	for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
		VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];
		const int mesh_block_size_at_lod = mesh_block_size << lod_index;

		for (const Box3i voxel_box : tls_modified_voxel_areas_lod0) {
			// Padding is required for edits near chunk borders, which can affect multiple meshes despite only affecting
			// one data block
			const Box3i padded_voxel_box = voxel_box.padded(1);
			const Box3i mesh_block_box = padded_voxel_box.downscaled(mesh_block_size_at_lod);

			mesh_block_box.for_each_cell([&lod](Vector3i mesh_block_pos) {
				auto mesh_block_it = lod.mesh_map_state.map.find(mesh_block_pos);
				if (mesh_block_it != lod.mesh_map_state.map.end()) {
					// If a mesh block state exists here, it will need an update.
					// If there is none, it will probably get created later when we come closer to it
					schedule_mesh_update( //
							mesh_block_it->second, //
							mesh_block_pos, //
							lod.mesh_blocks_pending_update, //
							mesh_block_it->second.mesh_viewers.get() > 0 //
					);
				}
			});
		}
	}

	// -- Old logic based solely on updated data blocks, however doesn't account for padding (would have to force-edit
	// around to simulate that, which isn't great).
	// Schedule mesh updates at every affected LOD
	// for (const VoxelData::BlockLocation loc : tls_updated_block_locations) {
	// 	const Vector3i mesh_block_pos = math::floordiv(loc.position, data_to_mesh_factor);
	// 	VoxelLodTerrainUpdateData::Lod &dst_lod = state.lods[loc.lod_index];
	//
	// 	auto mesh_block_it = dst_lod.mesh_map_state.map.find(mesh_block_pos);
	// 	if (mesh_block_it != dst_lod.mesh_map_state.map.end()) {
	// 		// If a mesh block state exists here, it will need an update.
	// 		// If there is none, it will probably get created later when we come closer to it
	// 		schedule_mesh_update(mesh_block_it->second, mesh_block_pos, dst_lod.blocks_pending_update);
	// 	}
	// }
}

uint8_t VoxelLodTerrainUpdateTask::get_transition_mask( //
		const VoxelLodTerrainUpdateData::State &state, //
		Vector3i block_pos, //
		unsigned int lod_index, //
		unsigned int lod_count //
) {
	uint8_t transition_mask = 0;

	if (lod_index + 1 >= lod_count) {
		// We do transitions on higher-resolution blocks.
		// Therefore, lowest-resolution blocks never have transitions.
		return transition_mask;
	}

	const VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];

	// Based on octree rules, and the fact it must have run before, check neighbor blocks of same LOD:
	// If one is missing or not visible, it means either of the following:
	// - The neighbor at lod+1 is visible or not loaded (there must be a transition)
	// - The neighbor at lod-1 is visible (no transition)

	uint8_t visible_neighbors_of_same_lod = 0;
	for (unsigned int dir = 0; dir < Cube::SIDE_COUNT; ++dir) {
		const Vector3i npos = block_pos + Cube::g_side_normals[dir];

		auto nblock_it = lod.mesh_map_state.map.find(npos);

		if (nblock_it != lod.mesh_map_state.map.end() && nblock_it->second.visual_active) {
			visible_neighbors_of_same_lod |= (1 << dir);
		}
	}

	if (visible_neighbors_of_same_lod == 0b111111) {
		// No transitions needed
		return transition_mask;
	}

	{
		const Vector3i lower_pos = block_pos >> 1;
		const Vector3i upper_pos = block_pos << 1;

		const VoxelLodTerrainUpdateData::Lod &lower_lod = state.lods[lod_index + 1];

		// At least one neighbor isn't visible.
		// Check for neighbors at different LOD (there can be only one kind on a given side)
		for (unsigned int dir = 0; dir < Cube::SIDE_COUNT; ++dir) {
			const unsigned int dir_mask = (1 << dir);

			if ((visible_neighbors_of_same_lod & dir_mask) != 0) {
				continue;
			}

			const Vector3i side_normal = Cube::g_side_normals[dir];
			const Vector3i lower_neighbor_pos = (block_pos + side_normal) >> 1;

			if (lower_neighbor_pos != lower_pos) {
				auto lower_neighbor_block_it = lower_lod.mesh_map_state.map.find(lower_neighbor_pos);

				if (lower_neighbor_block_it != lower_lod.mesh_map_state.map.end() &&
						lower_neighbor_block_it->second.visual_active) {
					// The block has a visible neighbor of lower LOD
					transition_mask |= dir_mask;
					continue;
				}
			}

			if (lod_index > 0) {
				// Check upper LOD neighbors.
				// There are always 4 on each side, checking any is enough

				Vector3i upper_neighbor_pos = upper_pos;
				for (unsigned int i = 0; i < Vector3iUtil::AXIS_COUNT; ++i) {
					if (side_normal[i] == -1) {
						--upper_neighbor_pos[i];
					} else if (side_normal[i] == 1) {
						upper_neighbor_pos[i] += 2;
					}
				}

				const VoxelLodTerrainUpdateData::Lod &upper_lod = state.lods[lod_index - 1];
				auto upper_neighbor_block_it = upper_lod.mesh_map_state.map.find(upper_neighbor_pos);

				if (upper_neighbor_block_it == upper_lod.mesh_map_state.map.end() ||
						upper_neighbor_block_it->second.visual_active == false) {
					// The block has no visible neighbor yet. World border? Assume lower LOD.
					transition_mask |= dir_mask;
				}
			}
		}
	}

	return transition_mask;
}

void update_transition_masks( //
		VoxelLodTerrainUpdateData::State &state, //
		uint32_t lods_to_update_transitions, //
		unsigned int lod_count, //
		// Currently needed to keep supporting the old octree streaming system, which doesn't support multiple viewers
		bool use_refcounts //
) {
	// TODO Optimize: this works but it's not smart.
	// It doesn't take too long (100 microseconds when octrees update with lod distance 60).
	// We used to only update positions based on which blocks were added/removed in the octree update,
	// which was faster than this. However it missed some spots, which caused annoying cracks to show up.
	// So instead, when any block changes state in LOD N, we update all transitions in LODs N-1, N, and N+1.
	// It is unclear yet why the old approach didn't work, maybe because it didn't properly made N-1 and N+1 update.
	// If you find a better approach, it has to comply with the validation check below.
	if (lods_to_update_transitions != 0) {
		ZN_PROFILE_SCOPE_NAMED("Transition masks");
		// We pass a mask that gets populated with (0b111 << index), because we want to add lod+1, lod+0 and lod-1. But
		// because the case of -1 would require more code, we instead offset the mask by 1. Then at the end, we
		// only need to undo that offset once here.
		lods_to_update_transitions >>= 1;

		for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
			if ((lods_to_update_transitions & (1 << lod_index)) == 0) {
				continue;
			}

			VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];

			// TODO Might not be necessary because we run this in the update task. No other thread is allowed to modify
			// this map while the task is running.
			RWLockRead rlock(lod.mesh_map_state.map_lock);

			for (auto it = lod.mesh_map_state.map.begin(); it != lod.mesh_map_state.map.end(); ++it) {
				VoxelLodTerrainUpdateData::MeshBlockState &mesh_block = it->second;

				if (mesh_block.visual_active && (!use_refcounts || mesh_block.mesh_viewers.get() > 0)) {
					const uint8_t recomputed_mask =
							VoxelLodTerrainUpdateTask::get_transition_mask(state, it->first, lod_index, lod_count);

					if (recomputed_mask != it->second.transition_mask) {
						mesh_block.transition_mask = recomputed_mask;
						lod.mesh_blocks_to_update_transitions.push_back(
								VoxelLodTerrainUpdateData::TransitionUpdate{ it->first, recomputed_mask });
					}
				}
			}
		}
	}
#if 0
	// DEBUG: Validation check for transition mask updates.
	{
		ZN_PROFILE_SCOPE_NAMED("Transition checks");
		for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
			const VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];
			RWLockRead rlock(lod.mesh_map_state.map_lock);
			for (auto it = lod.mesh_map_state.map.begin(); it != lod.mesh_map_state.map.end(); ++it) {
				if (it->second.active) {
					const uint8_t recomputed_mask =
							VoxelLodTerrainUpdateTask::get_transition_mask(state, it->first, lod_index, lod_count);
					CRASH_COND(recomputed_mask != it->second.transition_mask);
				}
			}
		}
	}
#endif
}

void add_unloaded_saving_blocks(VoxelLodTerrainUpdateData::Lod &lod, Span<const VoxelData::BlockToSave> src) {
	if (src.size() == 0) {
		return;
	}
	ZN_PROFILE_SCOPE();
	MutexLock mlock(lod.unloaded_saving_blocks_mutex);
	for (const VoxelData::BlockToSave &bts : src) {
		lod.unloaded_saving_blocks[bts.position] = bts.voxels;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void VoxelLodTerrainUpdateTask::run(ThreadedTaskContext &ctx) {
	ZN_PROFILE_SCOPE();

	struct SetCompleteOnScopeExit {
		std::atomic_bool &_complete;
		SetCompleteOnScopeExit(std::atomic_bool &b) : _complete(b) {}
		~SetCompleteOnScopeExit() {
			_complete = true;
		}
	};

#ifdef DEV_ENABLED
	CRASH_COND(_update_data == nullptr);
	CRASH_COND(_data == nullptr);
	CRASH_COND(_streaming_dependency == nullptr);
	CRASH_COND(_meshing_dependency == nullptr);
	CRASH_COND(_shared_viewers_data == nullptr);
#endif

	VoxelLodTerrainUpdateData &update_data = *_update_data;
	VoxelLodTerrainUpdateData::State &state = update_data.state;
	const VoxelLodTerrainUpdateData::Settings &settings = update_data.settings;
	VoxelData &data = *_data;
	Ref<VoxelGenerator> generator = _streaming_dependency->generator;
	Ref<VoxelStream> stream = _streaming_dependency->stream;
	ProfilingClock profiling_clock;
	ProfilingClock profiling_clock_total;

	// TODO This is not a good name, "streaming" has several meanings. Rename "can_load"?
	const bool stream_enabled = (stream.is_valid() || generator.is_valid()) &&
			(Engine::get_singleton()->is_editor_hint() == false || settings.run_stream_in_editor);

	const unsigned int lod_count = data.get_lod_count();

#ifdef DEV_ENABLED
	// Make sure the main thread has processed outputs of the last threaded update
	for (unsigned int lod_index = 0; lod_index < state.lods.size(); ++lod_index) {
		const VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];
		CRASH_COND(lod.mesh_blocks_to_unload.size() != 0);
		CRASH_COND(lod.mesh_blocks_to_update_transitions.size() != 0);
		CRASH_COND(lod.mesh_blocks_to_activate_visuals.size() != 0);
		CRASH_COND(lod.mesh_blocks_to_deactivate_visuals.size() != 0);
		CRASH_COND(lod.mesh_blocks_to_activate_collision.size() != 0);
		CRASH_COND(lod.mesh_blocks_to_deactivate_collision.size() != 0);
	}
#endif

	SetCompleteOnScopeExit scoped_complete(update_data.task_is_complete);

	CRASH_COND_MSG(update_data.task_is_complete, "Expected only one update task to run on a given volume");
	MutexLock mutex_lock(update_data.completion_mutex);

	// Update pending LOD data modifications due to edits.
	// These are deferred from edits so we can batch them.
	// It has to happen first because blocks can be unloaded afterwards.
	// This is also what causes meshes to update after edits.
	flush_pending_lod_edits(state, data, 1 << settings.mesh_block_size_po2);

	// Other mesh updates
	process_changed_generated_areas(state, settings, lod_count);

	static thread_local StdVector<VoxelData::BlockToSave> tls_data_blocks_to_save;
	static thread_local StdVector<VoxelLodTerrainUpdateData::BlockToLoad> tls_data_blocks_to_load;

	StdVector<VoxelLodTerrainUpdateData::BlockToLoad> &data_blocks_to_load = tls_data_blocks_to_load;
	data_blocks_to_load.clear();

	StdVector<VoxelData::BlockToSave> *data_blocks_to_save = stream.is_valid() ? &tls_data_blocks_to_save : nullptr;

	profiling_clock.restart();
	if (settings.streaming_system == VoxelLodTerrainUpdateData::STREAMING_SYSTEM_LEGACY_OCTREE) {
		process_octree_streaming( //
				state, //
				data, //
				_viewer_pos, //
				data_blocks_to_save, //
				data_blocks_to_load, //
				settings, //
				stream_enabled //
		);
	} else {
		process_clipbox_streaming( //
				state, //
				data, //
				to_span(update_data.viewers), //
				_volume_transform, //
				data_blocks_to_save, //
				data_blocks_to_load, //
				settings, //
				stream_enabled, //
				_meshing_dependency->mesher.is_valid() //
		);
	}
	state.stats.time_detect_required_blocks = profiling_clock.restart();

	BufferedTaskScheduler &task_scheduler = BufferedTaskScheduler::get_for_current_thread();

	process_async_edits( //
			state, //
			settings, //
			_data, //
			_volume_id, //
			_streaming_dependency, //
			_shared_viewers_data, //
			_volume_transform, //
			task_scheduler //
	);

	profiling_clock.restart();
	{
		ZN_PROFILE_SCOPE_NAMED("IO requests");
		// It's possible the user didn't set a stream yet, or it is turned off
		if (stream_enabled) {
			const unsigned int data_block_size = data.get_block_size();

			// This part would still "work" without that check because `data_blocks_to_load` would be empty,
			// but I added this for expliciteness
			if (data.is_streaming_enabled()) {
				if (stream.is_null() && !settings.cache_generated_blocks) {
					// TODO Optimization: not ideal because a bit delayed. It requires a second update cycle for meshes
					// to get requested. We could instead set those empty blocks right away instead of putting them in
					// that list, but it's simpler code for now.
					apply_block_data_requests_as_empty(to_span(data_blocks_to_load), data, state, settings);

				} else {
					send_block_data_requests( //
							_volume_id, //
							to_span(data_blocks_to_load), //
							_streaming_dependency, //
							_data, //
							_shared_viewers_data, //
							data_block_size, //
							_volume_transform, //
							settings, //
							task_scheduler, //
							state //
					);
				}
			}

			if (data_blocks_to_save != nullptr) {
				send_block_save_requests( //
						_volume_id, //
						to_span(*data_blocks_to_save), //
						_streaming_dependency, //
						task_scheduler, //
						nullptr, //
						false //
				);
			}
		}
		data_blocks_to_load.clear();
		if (data_blocks_to_save != nullptr) {
			data_blocks_to_save->clear();
		}
	}
	state.stats.time_io_requests = profiling_clock.restart();

	// TODO When no mesher is assigned, mesh requests are still accumulated but not being sent. A better way to support
	// this is by allowing voxels-only/mesh-less viewers, similar to VoxelTerrain
	if (_meshing_dependency->mesher.is_valid()) {
		send_mesh_requests( //
				_volume_id, //
				state, //
				settings, //
				_data, //
				_meshing_dependency, //
				_shared_viewers_data, //
				_volume_transform, //
				task_scheduler //
		);
	}

	task_scheduler.flush();

	state.stats.time_mesh_requests = profiling_clock.restart();

	state.stats.time_total = profiling_clock.restart();
}

} // namespace zylann::voxel
