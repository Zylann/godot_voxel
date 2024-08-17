#include "generate_block_multipass_cb_task.h"
#include "../../engine/voxel_engine.h"
#include "../../generators/multipass/generate_column_multipass_task.h"
#include "../../generators/multipass/voxel_generator_multipass_cb.h"
#include "../../storage/voxel_buffer.h"
#include "../../storage/voxel_data.h"
#include "../../streams/save_block_data_task.h"
#include "../../util/dstack.h"
#include "../../util/godot/classes/time.h"
#include "../../util/io/log.h"
#include "../../util/math/conv.h"
#include "../../util/profiling.h"
#include "../../util/string/format.h"
#include "../../util/tasks/async_dependency_tracker.h"

namespace zylann::voxel {

GenerateBlockMultipassCBTask::GenerateBlockMultipassCBTask(const VoxelGenerator::BlockTaskParams &params) :
		_block_position(params.block_position),
		_volume_id(params.volume_id),
		_lod_index(params.lod_index),
		_block_size(params.block_size),
		_drop_beyond_max_distance(params.drop_beyond_max_distance),
		_priority_dependency(params.priority_dependency),
		_stream_dependency(params.stream_dependency),
		_tracker(params.tracker),
		_cancellation_token(params.cancellation_token) {
	//
	VoxelEngine::get_singleton().debug_increment_generate_block_task_counter();
}

GenerateBlockMultipassCBTask::~GenerateBlockMultipassCBTask() {
	VoxelEngine::get_singleton().debug_decrement_generate_block_task_counter();
	// println(format("H {} {} {} {}", position.x, position.y, position.z, Time::get_singleton()->get_ticks_usec()));
}

void GenerateBlockMultipassCBTask::run(zylann::ThreadedTaskContext &ctx) {
	ZN_DSTACK();
	ZN_PROFILE_SCOPE();

	CRASH_COND(_stream_dependency == nullptr);
	Ref<VoxelGenerator> generator = _stream_dependency->generator;
	ERR_FAIL_COND(generator.is_null());

	// TODO Have a way for generators to provide their own task, instead of shoehorning it here
	Ref<VoxelGeneratorMultipassCB> multipass_generator = generator;
	ZN_ASSERT_RETURN(multipass_generator.is_valid());

	ZN_ASSERT_RETURN(multipass_generator->get_pass_count() > 0);
	std::shared_ptr<VoxelGeneratorMultipassCBStructs::Internal> multipass_generator_internal =
			multipass_generator->get_internal();
	VoxelGeneratorMultipassCBStructs::Map &map = multipass_generator_internal->map;

	const int final_subpass_index =
			VoxelGeneratorMultipassCB::get_subpass_count_from_pass_count(multipass_generator->get_pass_count()) - 1;

	const int column_min_y = multipass_generator->get_column_base_y_blocks();
	const int column_height = multipass_generator->get_column_height_blocks();

	if (_block_position.y < column_min_y || _block_position.y >= column_min_y + column_height) {
		// Fallback on single pass

	} else {
		const Vector2i column_position(_block_position.x, _block_position.z);
		// TODO Candidate for postponing? Lots of them, might cause contention
		SpatialLock2D::Read srlock(map.spatial_lock, BoxBounds2i::from_position(column_position));
		VoxelGeneratorMultipassCBStructs::Column *column = nullptr;
		{
			MutexLock mlock(map.mutex);
			auto column_it = map.columns.find(column_position);
			if (column_it == map.columns.end()) {
				// Drop, for some reason it wasn't available
				return;
			}

			column = &column_it->second;
		}

		if (column == nullptr) {
			// Drop.
			// Either we returned here after subtasks got cancelled, or the target column simply got unloaded before
			// the task started running.
			return;
		}

		const int block_index = _block_position.y - column_min_y;

		if (column->subpass_index != final_subpass_index) {
			// The block isn't finished
			if (_stage == 1) {
				// We came back here after having scheduled subtasks, means they had to cancel.
				// Drop
				// TODO We really need to find a way to streamline this coroutine-like task workflow
				return;
			}

			VoxelGeneratorMultipassCBStructs::Block &block = column->blocks[block_index];

			if (block.final_pending_task != nullptr) {
				// There is already a generate task for that block.

				// It must not be the current task. Only tasks that are not scheduled and not running can be stored
				// in here. If it is, something went wrong.
				ZN_ASSERT(block.final_pending_task != this);

				// This can happen if you teleport forward, then go back, then forward again.
				// Because VoxelTerrain forgets about "loading blocks" falling out of its play area, while
				// multipass generators forget about them in a larger radius, therefore causing the same block
				// to be requested again while the previous request is still cached, not having run yet.
				// We may not delete this task.

				// Drop current task, we should get the work done by the existing task
				return;
				// TODO Not sure if we should let the terrain see this as a drop if there is an existing task!
				// It could cause a request loop even though a task already is pending
			}

			if ((column->pending_subpass_tasks_mask & (1 << final_subpass_index)) == 0) {
				// No tasks working on it, and we are the first top-level task.
				// Spawn a subtask to bring this column to final state.
				GenerateColumnMultipassTask *subtask = ZN_NEW(GenerateColumnMultipassTask(
						column_position,
						_block_size,
						final_subpass_index,
						multipass_generator_internal,
						multipass_generator,
						ctx.task_priority,
						// The subtask takes ownership of the current task, it will schedule it back when it
						// finishes (or cancels)
						this,
						make_shared_instance<std::atomic_int>(1)
				));

				column->pending_subpass_tasks_mask |= (1 << final_subpass_index);

				VoxelEngine::get_singleton().push_async_task(subtask);

			} else {
				// A column task is already underway.
				// Register as waiting for this column's final result. The block takes ownership.
				block.final_pending_task = this;
			}

			// Ownership is now either given to a subtask or the desired block.
			// As such, we must be taken out of the task runner. We'll get scheduled again when the column is
			// finished, or when it gets unloaded.
			ctx.status = ThreadedTaskContext::STATUS_TAKEN_OUT;
			_stage = 1;
			// println(format("Takeout {}", uint64_t(this)));

		} else {
			// The block is ready

			VoxelGeneratorMultipassCBStructs::Block &block = column->blocks[block_index];

			// TODO Take out voxel data from this block to save memory, it must not be touched by generation anymore
			// (if we do, we need to change the column's spatial lock to WRITE)
			voxels = make_shared_instance<VoxelBuffer>(VoxelBuffer::ALLOCATOR_POOL);
			voxels->create(block.voxels.get_size());
			voxels->copy_channels_from(block.voxels);
			// TODO Metadata?

			run_stream_saving_and_finish();
		}
		return;
	}

	// Single-pass generation

	if (voxels == nullptr) {
		voxels = make_shared_instance<VoxelBuffer>(VoxelBuffer::ALLOCATOR_POOL);
		voxels->create(_block_size, _block_size, _block_size);
	}

	run_cpu_generation();
	run_stream_saving_and_finish();
}

void GenerateBlockMultipassCBTask::run_cpu_generation() {
	const Vector3i origin_in_voxels = (_block_position << _lod_index) * _block_size;

	Ref<VoxelGenerator> generator = _stream_dependency->generator;

	VoxelGenerator::VoxelQueryData query_data{ *voxels, origin_in_voxels, _lod_index };
	generator->generate_block(query_data);
}

void GenerateBlockMultipassCBTask::run_stream_saving_and_finish() {
	if (_stream_dependency->valid) {
		Ref<VoxelStream> stream = _stream_dependency->stream;

		// TODO In some cases we don't want this to run all the time, do we?
		// Like in full load mode, where non-edited blocks remain generated on the fly...
		if (stream.is_valid() && stream->get_save_generator_output()) {
			ZN_PRINT_VERBOSE(
					format("Requesting save of generator output for block {} lod {}", _block_position, int(_lod_index))
			);

			// TODO Optimization: `voxels` doesn't actually need to be shared
			std::shared_ptr<VoxelBuffer> voxels_copy = make_shared_instance<VoxelBuffer>(VoxelBuffer::ALLOCATOR_POOL);
			voxels->copy_to(*voxels_copy, true);

			// No instances, generators are not designed to produce them at this stage yet.
			// No priority data, saving doesn't need sorting.

			SaveBlockDataTask *save_task = ZN_NEW(SaveBlockDataTask(
					_volume_id, _block_position, _lod_index, voxels_copy, _stream_dependency, nullptr, false
			));

			VoxelEngine::get_singleton().push_async_io_task(save_task);
		}
	}

	_has_run = true;
}

TaskPriority GenerateBlockMultipassCBTask::get_priority() {
	float closest_viewer_distance_sq;
	const TaskPriority p = _priority_dependency.evaluate(
			_lod_index, constants::TASK_PRIORITY_GENERATE_BAND2, &closest_viewer_distance_sq
	);
	_too_far = _drop_beyond_max_distance && closest_viewer_distance_sq > _priority_dependency.drop_distance_squared;
	return p;
}

bool GenerateBlockMultipassCBTask::is_cancelled() {
	if (_stream_dependency->valid == false) {
		return true;
	}
	if (_cancellation_token.is_valid()) {
		return _cancellation_token.is_cancelled();
	}
	return _too_far; // || stream_dependency->stream->get_fallback_generator().is_null();
}

void GenerateBlockMultipassCBTask::apply_result() {
	bool aborted = true;

	if (VoxelEngine::get_singleton().is_volume_valid(_volume_id)) {
		// TODO Comparing pointer may not be guaranteed
		// The request response must match the dependency it would have been requested with.
		// If it doesn't match, we are no longer interested in the result.
		if (_stream_dependency->valid) {
			Ref<VoxelStream> stream = _stream_dependency->stream;

			VoxelEngine::BlockDataOutput o;
			o.voxels = voxels;
			o.position = _block_position;
			o.lod_index = _lod_index;
			o.dropped = !_has_run;
			if (stream.is_valid() && stream->get_save_generator_output()) {
				// We can't consider the block as "generated" since there is no state to tell that once saved,
				// so it has to be considered an edited block
				o.type = VoxelEngine::BlockDataOutput::TYPE_LOADED;
			} else {
				o.type = VoxelEngine::BlockDataOutput::TYPE_GENERATED;
			}
			o.max_lod_hint = false;
			o.initial_load = false;

			VoxelEngine::VolumeCallbacks callbacks = VoxelEngine::get_singleton().get_volume_callbacks(_volume_id);
			ERR_FAIL_COND(callbacks.data_output_callback == nullptr);
			callbacks.data_output_callback(callbacks.data, o);

			aborted = !_has_run;
		}

	} else {
		// This can happen if the user removes the volume while requests are still about to return
		ZN_PRINT_VERBOSE("Gemerated data request response came back but volume wasn't found");
	}

	// TODO We could complete earlier inside run() if we had access to the data structure to write the block into.
	// This would reduce latency a little. The rest of things the terrain needs to do with the generated block could
	// run later.
	if (_tracker != nullptr) {
		if (aborted) {
			_tracker->abort();
		} else {
			_tracker->post_complete();
		}
	}
}

} // namespace zylann::voxel
