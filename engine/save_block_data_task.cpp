#include "save_block_data_task.h"
#include "../storage/voxel_buffer_internal.h"
#include "../util/log.h"
#include "../util/profiling.h"
#include "../util/string_funcs.h"
#include "../util/tasks/async_dependency_tracker.h"
#include "generate_block_task.h"
#include "voxel_engine.h"

namespace zylann::voxel {

namespace {
std::atomic_int g_debug_save_block_tasks_count = { 0 };
}

SaveBlockDataTask::SaveBlockDataTask(VolumeID p_volume_id, Vector3i p_block_pos, uint8_t p_lod, uint8_t p_block_size,
		std::shared_ptr<VoxelBufferInternal> p_voxels, std::shared_ptr<StreamingDependency> p_stream_dependency,
		std::shared_ptr<AsyncDependencyTracker> p_tracker) :
		_voxels(p_voxels),
		_position(p_block_pos),
		_volume_id(p_volume_id),
		_lod(p_lod),
		_block_size(p_block_size),
		_save_instances(false),
		_save_voxels(true),
		_stream_dependency(p_stream_dependency),
		_tracker(p_tracker) {
	//
	++g_debug_save_block_tasks_count;
}

SaveBlockDataTask::SaveBlockDataTask(VolumeID p_volume_id, Vector3i p_block_pos, uint8_t p_lod, uint8_t p_block_size,
		UniquePtr<InstanceBlockData> p_instances, std::shared_ptr<StreamingDependency> p_stream_dependency,
		std::shared_ptr<AsyncDependencyTracker> p_tracker) :
		_instances(std::move(p_instances)),
		_position(p_block_pos),
		_volume_id(p_volume_id),
		_lod(p_lod),
		_block_size(p_block_size),
		_save_instances(true),
		_save_voxels(false),
		_stream_dependency(p_stream_dependency),
		_tracker(p_tracker) {
	//
	++g_debug_save_block_tasks_count;
}

SaveBlockDataTask::~SaveBlockDataTask() {
	--g_debug_save_block_tasks_count;
}

int SaveBlockDataTask::debug_get_running_count() {
	return g_debug_save_block_tasks_count;
}

void SaveBlockDataTask::run(zylann::ThreadedTaskContext ctx) {
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

		VoxelBufferInternal voxels_copy;
		{
			RWLockRead lock(_voxels->get_lock());
			// TODO Optimization: is that copy necessary? It's possible it was already done while issuing the
			// request
			_voxels->duplicate_to(voxels_copy, true);
		}
		_voxels = nullptr;
		const Vector3i origin_in_voxels = (_position << _lod) * _block_size;
		VoxelStream::VoxelQueryData q{ voxels_copy, origin_in_voxels, _lod, VoxelStream::RESULT_ERROR };
		stream->save_voxel_block(q);
	}

	if (_save_instances && stream->supports_instance_blocks()) {
		// If the provided data is null, it means this instance block was never modified.
		// Since we are in a save request, the saved data will revert to unmodified.
		// On the other hand, if we want to represent the fact that "everything was deleted here",
		// this should not be null.

		ZN_PRINT_VERBOSE(format("Saving instance block {} lod {} with data {}", _position, _lod, _instances.get()));

		VoxelStream::InstancesQueryData instances_query{ std::move(_instances), _position, _lod,
			VoxelStream::RESULT_ERROR };
		stream->save_instance_blocks(Span<VoxelStream::InstancesQueryData>(&instances_query, 1));
	}

	if (_tracker != nullptr) {
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
			o.lod = _lod;
			o.dropped = !_has_run;
			o.max_lod_hint = false; // Unused
			o.initial_load = false; // Unused
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
