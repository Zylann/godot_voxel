#include "generate_block_task.h"
#include "../storage/voxel_buffer_internal.h"
#include "../storage/voxel_data.h"
#include "../util/godot/funcs.h"
#include "../util/log.h"
#include "../util/profiling.h"
#include "../util/string_funcs.h"
#include "../util/tasks/async_dependency_tracker.h"
#include "save_block_data_task.h"
#include "voxel_engine.h"

namespace zylann::voxel {

namespace {
std::atomic_int g_debug_generate_tasks_count = { 0 };
}

GenerateBlockTask::GenerateBlockTask() {
	++g_debug_generate_tasks_count;
}

GenerateBlockTask::~GenerateBlockTask() {
	--g_debug_generate_tasks_count;
}

int GenerateBlockTask::debug_get_running_count() {
	return g_debug_generate_tasks_count;
}

void GenerateBlockTask::run(zylann::ThreadedTaskContext ctx) {
	ZN_PROFILE_SCOPE();

	CRASH_COND(stream_dependency == nullptr);
	Ref<VoxelGenerator> generator = stream_dependency->generator;
	ERR_FAIL_COND(generator.is_null());

	const Vector3i origin_in_voxels = (position << lod) * block_size;

	if (voxels == nullptr) {
		voxels = make_shared_instance<VoxelBufferInternal>();
		voxels->create(block_size, block_size, block_size);
	}

	VoxelGenerator::VoxelQueryData query_data{ *voxels, origin_in_voxels, lod };
	const VoxelGenerator::Result result = generator->generate_block(query_data);
	max_lod_hint = result.max_lod_hint;

	if (data != nullptr) {
		data->get_modifiers().apply(
				query_data.voxel_buffer, AABB(query_data.origin_in_voxels, query_data.voxel_buffer.get_size() << lod));
	}

	if (stream_dependency->valid) {
		Ref<VoxelStream> stream = stream_dependency->stream;

		// TODO In some cases we don't want this to run all the time, do we?
		// Like in full load mode, where non-edited blocks remain generated on the fly...
		if (stream.is_valid() && stream->get_save_generator_output()) {
			ZN_PRINT_VERBOSE(format("Requesting save of generator output for block {} lod {}", position, int(lod)));

			// TODO Optimization: `voxels` doesn't actually need to be shared
			std::shared_ptr<VoxelBufferInternal> voxels_copy = make_shared_instance<VoxelBufferInternal>();
			voxels->duplicate_to(*voxels_copy, true);

			// No instances, generators are not designed to produce them at this stage yet.
			// No priority data, saving doesn't need sorting.

			SaveBlockDataTask *save_task = memnew(
					SaveBlockDataTask(volume_id, position, lod, block_size, voxels_copy, stream_dependency, nullptr));

			VoxelEngine::get_singleton().push_async_io_task(save_task);
		}
	}

	has_run = true;
}

TaskPriority GenerateBlockTask::get_priority() {
	float closest_viewer_distance_sq;
	const TaskPriority p =
			priority_dependency.evaluate(lod, constants::TASK_PRIORITY_GENERATE_BAND2, &closest_viewer_distance_sq);
	too_far = drop_beyond_max_distance && closest_viewer_distance_sq > priority_dependency.drop_distance_squared;
	return p;
}

bool GenerateBlockTask::is_cancelled() {
	return !stream_dependency->valid || too_far; // || stream_dependency->stream->get_fallback_generator().is_null();
}

void GenerateBlockTask::apply_result() {
	bool aborted = true;

	if (VoxelEngine::get_singleton().is_volume_valid(volume_id)) {
		// TODO Comparing pointer may not be guaranteed
		// The request response must match the dependency it would have been requested with.
		// If it doesn't match, we are no longer interested in the result.
		if (stream_dependency->valid) {
			Ref<VoxelStream> stream = stream_dependency->stream;

			VoxelEngine::BlockDataOutput o;
			o.voxels = voxels;
			o.position = position;
			o.lod = lod;
			o.dropped = !has_run;
			if (stream.is_valid() && stream->get_save_generator_output()) {
				// We can't consider the block as "generated" since there is no state to tell that once saved,
				// so it has to be considered an edited block
				o.type = VoxelEngine::BlockDataOutput::TYPE_LOADED;
			} else {
				o.type = VoxelEngine::BlockDataOutput::TYPE_GENERATED;
			}
			o.max_lod_hint = max_lod_hint;
			o.initial_load = false;

			VoxelEngine::VolumeCallbacks callbacks = VoxelEngine::get_singleton().get_volume_callbacks(volume_id);
			ERR_FAIL_COND(callbacks.data_output_callback == nullptr);
			callbacks.data_output_callback(callbacks.data, o);

			aborted = !has_run;
		}

	} else {
		// This can happen if the user removes the volume while requests are still about to return
		ZN_PRINT_VERBOSE("Gemerated data request response came back but volume wasn't found");
	}

	// TODO We could complete earlier inside run() if we had access to the data structure to write the block into.
	// This would reduce latency a little. The rest of things the terrain needs to do with the generated block could
	// run later.
	if (tracker != nullptr) {
		if (aborted) {
			tracker->abort();
		} else {
			tracker->post_complete();
		}
	}
}

} // namespace zylann::voxel
