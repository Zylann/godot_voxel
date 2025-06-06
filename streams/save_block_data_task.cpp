#include "save_block_data_task.h"
#include "../engine/voxel_engine.h"
#include "../generators/generate_block_task.h"
#include "../storage/voxel_buffer.h"
#include "../util/godot/core/string.h"
#include "../util/io/log.h"
#include "../util/profiling.h"
#include "../util/string/format.h"
#include "../util/tasks/async_dependency_tracker.h"

namespace zylann::voxel {

namespace {
std::atomic_int g_debug_save_block_tasks_count = { 0 };
}

SaveBlockDataTask::SaveBlockDataTask(
		VolumeID p_volume_id,
		Vector3i p_block_pos,
		uint8_t p_lod,
		std::shared_ptr<VoxelBuffer> p_voxels,
		std::shared_ptr<StreamingDependency> p_stream_dependency,
		std::shared_ptr<AsyncDependencyTracker> p_tracker,
		bool flush_on_last_tracked_task
) :
		_voxels(p_voxels),
		_position(p_block_pos),
		_volume_id(p_volume_id),
		_lod(p_lod),
		_save_instances(false),
		_save_voxels(true),
		_flush_on_last_tracked_task(flush_on_last_tracked_task),
		_stream_dependency(p_stream_dependency),
		_tracker(p_tracker) {
	//
	++g_debug_save_block_tasks_count;
}

#ifdef VOXEL_ENABLE_INSTANCER

SaveBlockDataTask::SaveBlockDataTask(
		VolumeID p_volume_id,
		Vector3i p_block_pos,
		uint8_t p_lod,
		UniquePtr<InstanceBlockData> p_instances,
		std::shared_ptr<StreamingDependency> p_stream_dependency,
		std::shared_ptr<AsyncDependencyTracker> p_tracker,
		bool flush_on_last_tracked_task
) :
		_instances(std::move(p_instances)),
		_position(p_block_pos),
		_volume_id(p_volume_id),
		_lod(p_lod),
		_save_instances(true),
		_save_voxels(false),
		_flush_on_last_tracked_task(flush_on_last_tracked_task),
		_stream_dependency(p_stream_dependency),
		_tracker(p_tracker) {
	//
	++g_debug_save_block_tasks_count;
}

#endif

SaveBlockDataTask::~SaveBlockDataTask() {
	--g_debug_save_block_tasks_count;
}

int SaveBlockDataTask::debug_get_running_count() {
	return g_debug_save_block_tasks_count;
}

void SaveBlockDataTask::run(zylann::ThreadedTaskContext &ctx) {
	ZN_PROFILE_SCOPE();

	CRASH_COND(_stream_dependency == nullptr);
	Ref<VoxelStream> stream = _stream_dependency->stream;
	ZN_ASSERT_RETURN_MSG(stream.is_valid(), "Save task was triggered without a stream, this is a bug");

	if (_save_voxels) {
		if (_voxels == nullptr) {
			if (_tracker != nullptr) {
				_tracker->abort();
			}
			ZN_PRINT_ERROR("Voxels to save shouldn't be null");
			return;
		}

		VoxelBuffer voxels_copy(VoxelBuffer::ALLOCATOR_POOL);
		// Note, we are not locking voxels here. This is supposed to be done at the time this task is scheduled.
		// If this is not a copy, it means the map it came from is getting unloaded anyways.
		// TODO Optimization: is that copy necessary? It's possible it was already done while issuing the
		// request
		_voxels->copy_to(voxels_copy, true);
		_voxels = nullptr;
		VoxelStream::VoxelQueryData q{ voxels_copy, _position, _lod, VoxelStream::RESULT_ERROR };
		stream->save_voxel_block(q);
	}

#ifdef VOXEL_ENABLE_INSTANCER
	if (_save_instances) {
		if (stream->supports_instance_blocks()) {
			// If the provided data is null, it means this instance block was never modified.
			// Since we are in a save request, the saved data will revert to unmodified.
			// On the other hand, if we want to represent the fact that "everything was deleted here",
			// this should not be null.

			ZN_PRINT_VERBOSE(format(
					"Saving instance block {} lod {} with data {}", _position, static_cast<int>(_lod), _instances.get()
			));

			VoxelStream::InstancesQueryData instances_query{
				std::move(_instances), _position, _lod, VoxelStream::RESULT_ERROR
			};
			stream->save_instance_blocks(Span<VoxelStream::InstancesQueryData>(&instances_query, 1));

		} else {
			ZN_PRINT_WARNING_ONCE(
					format("Tried to save instance block, but {} does not support them.", String(stream->get_class()))
			);
		}
	}
#endif

	if (_tracker != nullptr) {
		if (_flush_on_last_tracked_task && _tracker->get_remaining_count() == 1) {
			// This was the last task in a tracked group of saving tasks, we may flush now
			stream->flush();
		}
		_tracker->post_complete();
	}

	_has_run = true;
}

TaskPriority SaveBlockDataTask::get_priority() {
	TaskPriority p;
	p.band2 = constants::TASK_PRIORITY_SAVE_BAND2;
	p.band3 = constants::TASK_PRIORITY_BAND3_DEFAULT;
	return p;
}

bool SaveBlockDataTask::is_cancelled() {
	return false;
}

void SaveBlockDataTask::apply_result() {
	if (VoxelEngine::get_singleton().is_volume_valid(_volume_id)) {
		if (_stream_dependency->valid) {
			// TODO Perhaps separate save and load callbacks?
			VoxelEngine::BlockDataOutput o;
			o.position = _position;
			o.lod_index = _lod;
			o.dropped = !_has_run;
			o.max_lod_hint = false; // Unused
			o.initial_load = false; // Unused
			o.had_instances = _save_instances;
			o.had_voxels = _save_voxels;
			o.type = VoxelEngine::BlockDataOutput::TYPE_SAVED;

			VoxelEngine::VolumeCallbacks callbacks = VoxelEngine::get_singleton().get_volume_callbacks(_volume_id);
			CRASH_COND(callbacks.data_output_callback == nullptr);
			callbacks.data_output_callback(callbacks.data, o);
		}

	} else {
		// This can happen if the user removes the volume while requests are still about to return
		ZN_PRINT_VERBOSE("Stream data request response came back but volume wasn't found");
	}
}

} // namespace zylann::voxel
