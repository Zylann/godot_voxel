#include "block_generate_request.h"
#include "../util/godot/funcs.h"
#include "../util/macros.h"
#include "../util/profiling.h"
#include "block_data_request.h"
#include "voxel_server.h"

namespace zylann::voxel {

namespace {
std::atomic_int g_debug_generate_tasks_count;
}

BlockGenerateRequest::BlockGenerateRequest() {
	++g_debug_generate_tasks_count;
}

BlockGenerateRequest::~BlockGenerateRequest() {
	--g_debug_generate_tasks_count;
}

int BlockGenerateRequest::debug_get_running_count() {
	return g_debug_generate_tasks_count;
}

void BlockGenerateRequest::run(zylann::ThreadedTaskContext ctx) {
	VOXEL_PROFILE_SCOPE();

	CRASH_COND(stream_dependency == nullptr);
	Ref<VoxelGenerator> generator = stream_dependency->generator;
	ERR_FAIL_COND(generator.is_null());

	const Vector3i origin_in_voxels = (position << lod) * block_size;

	if (voxels == nullptr) {
		voxels = gd_make_shared<VoxelBufferInternal>();
		voxels->create(block_size, block_size, block_size);
	}

	VoxelBlockRequest r{ *voxels, origin_in_voxels, lod };
	const VoxelGenerator::Result result = generator->generate_block(r);
	max_lod_hint = result.max_lod_hint;

	if (stream_dependency->valid) {
		Ref<VoxelStream> stream = stream_dependency->stream;

		// TODO In some cases we dont want this to run all the time, do we?
		// Like in full load mode, where non-edited blocks remain generated on the fly...
		if (stream.is_valid() && stream->get_save_generator_output()) {
			PRINT_VERBOSE(
					String("Requesting save of generator output for block {0} lod {1}").format(varray(position, lod)));

			// TODO Optimization: `voxels` doesnt actually need to be shared
			std::shared_ptr<VoxelBufferInternal> voxels_copy = gd_make_shared<VoxelBufferInternal>();
			voxels->duplicate_to(*voxels_copy, true);

			// No instances, generators are not designed to produce them at this stage yet.
			// No priority data, saving doesnt need sorting

			BlockDataRequest *r =
					memnew(BlockDataRequest(volume_id, position, lod, block_size, voxels_copy, stream_dependency));

			VoxelServer::get_singleton()->push_async_task(r);
		}
	}

	has_run = true;
}

int BlockGenerateRequest::get_priority() {
	float closest_viewer_distance_sq;
	const int p = priority_dependency.evaluate(lod, &closest_viewer_distance_sq);
	too_far = drop_beyond_max_distance && closest_viewer_distance_sq > priority_dependency.drop_distance_squared;
	return p;
}

bool BlockGenerateRequest::is_cancelled() {
	return !stream_dependency->valid || too_far; // || stream_dependency->stream->get_fallback_generator().is_null();
}

void BlockGenerateRequest::apply_result() {
	bool aborted = true;

	if (VoxelServer::get_singleton()->is_volume_valid(volume_id)) {
		// TODO Comparing pointer may not be guaranteed
		// The request response must match the dependency it would have been requested with.
		// If it doesn't match, we are no longer interested in the result.
		if (stream_dependency->valid) {
			VoxelServer::BlockDataOutput o;
			o.voxels = voxels;
			o.position = position;
			o.lod = lod;
			o.dropped = !has_run;
			o.type = VoxelServer::BlockDataOutput::TYPE_GENERATED;
			o.max_lod_hint = max_lod_hint;
			o.initial_load = false;

			VoxelServer::VolumeCallbacks callbacks = VoxelServer::get_singleton()->get_volume_callbacks(volume_id);
			ERR_FAIL_COND(callbacks.data_output_callback == nullptr);
			callbacks.data_output_callback(callbacks.data, o);

			aborted = !has_run;
		}

	} else {
		// This can happen if the user removes the volume while requests are still about to return
		PRINT_VERBOSE("Gemerated data request response came back but volume wasn't found");
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
