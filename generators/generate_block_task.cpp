#include "generate_block_task.h"
#include "../engine/voxel_engine.h"
#include "../storage/voxel_buffer.h"
#include "../storage/voxel_data.h"
#include "../streams/save_block_data_task.h"
#include "../util/dstack.h"
#include "../util/io/log.h"
#include "../util/math/conv.h"
#include "../util/profiling.h"
#include "../util/string_funcs.h"
#include "../util/tasks/async_dependency_tracker.h"

namespace zylann::voxel {

GenerateBlockTask::GenerateBlockTask(const VoxelGenerator::BlockTaskParams &params) :
		_voxels(params.voxels),
		_position(params.block_position),
		_volume_id(params.volume_id),
		_lod_index(params.lod_index),
		_block_size(params.block_size),
		_drop_beyond_max_distance(params.drop_beyond_max_distance),
		_use_gpu(params.use_gpu),
		_priority_dependency(params.priority_dependency),
		_stream_dependency(params.stream_dependency),
		_data(params.data),
		_tracker(params.tracker),
		_cancellation_token(params.cancellation_token) {
	//
	VoxelEngine::get_singleton().debug_increment_generate_block_task_counter();
	// println(format(
	// 		"G {} {} {} {}", block_pos.x, block_pos.y, block_pos.z, Time::get_singleton()->get_ticks_usec()));
}

GenerateBlockTask::~GenerateBlockTask() {
	VoxelEngine::get_singleton().debug_decrement_generate_block_task_counter();
	// println(format("H {} {} {} {}", position.x, position.y, position.z, Time::get_singleton()->get_ticks_usec()));
}

void GenerateBlockTask::run(zylann::ThreadedTaskContext &ctx) {
	ZN_DSTACK();
	ZN_PROFILE_SCOPE();

	CRASH_COND(_stream_dependency == nullptr);
	Ref<VoxelGenerator> generator = _stream_dependency->generator;
	ERR_FAIL_COND(generator.is_null());

	if (_voxels == nullptr) {
		_voxels = make_shared_instance<VoxelBuffer>();
		_voxels->create(_block_size, _block_size, _block_size);
	}

	if (_use_gpu) {
		if (_stage == 0) {
			run_gpu_task(ctx);
		}
		if (_stage == 1) {
			run_gpu_conversion();
		}
		if (_stage == 2) {
			run_stream_saving_and_finish();
		}
	} else {
		run_cpu_generation();
		run_stream_saving_and_finish();
	}
}

void GenerateBlockTask::run_gpu_task(zylann::ThreadedTaskContext &ctx) {
	Ref<VoxelGenerator> generator = _stream_dependency->generator;
	ERR_FAIL_COND(generator.is_null());

	// TODO Broad-phase to avoid the GPU part entirely?
	// Implement and call `VoxelGenerator::generate_broad_block()`

	std::shared_ptr<ComputeShader> generator_shader = generator->get_block_rendering_shader();
	ERR_FAIL_COND(generator_shader == nullptr);

	const Vector3i origin_in_voxels = (_position << _lod_index) * _block_size;

	ZN_ASSERT(_voxels != nullptr);
	VoxelGenerator::VoxelQueryData generator_query{ *_voxels, origin_in_voxels, _lod_index };
	if (generator->generate_broad_block(generator_query)) {
		_stage = 2;
		return;
	}

	const Vector3i resolution = Vector3iUtil::create(_block_size);

	GenerateBlockGPUTask *gpu_task = ZN_NEW(GenerateBlockGPUTask);
	gpu_task->boxes_to_generate.push_back(Box3i(Vector3i(), resolution));
	gpu_task->generator_shader = generator_shader;
	gpu_task->generator_shader_params = generator->get_block_rendering_shader_parameters();
	gpu_task->generator_shader_outputs = generator->get_block_rendering_shader_outputs();
	gpu_task->lod_index = _lod_index;
	gpu_task->origin_in_voxels = origin_in_voxels;
	gpu_task->consumer_task = this;

	if (_data != nullptr) {
		const AABB aabb_voxels(to_vec3(origin_in_voxels), to_vec3(resolution << _lod_index));
		StdVector<VoxelModifier::ShaderData> modifiers_shader_data;
		const VoxelModifierStack &modifiers = _data->get_modifiers();
		modifiers.apply_for_gpu_rendering(modifiers_shader_data, aabb_voxels, VoxelModifier::ShaderData::TYPE_BLOCK);
		for (const VoxelModifier::ShaderData &d : modifiers_shader_data) {
			gpu_task->modifiers.push_back(GenerateBlockGPUTask::ModifierData{
					d.shader_rids[VoxelModifier::ShaderData::TYPE_BLOCK], d.params });
		}
	}

	ctx.status = ThreadedTaskContext::STATUS_TAKEN_OUT;

	// Start GPU task, we'll continue after it
	VoxelEngine::get_singleton().push_gpu_task(gpu_task);
}

void GenerateBlockTask::set_gpu_results(StdVector<GenerateBlockGPUTaskResult> &&results) {
	_gpu_generation_results = std::move(results);
	_stage = 1;
}

void GenerateBlockTask::run_gpu_conversion() {
	GenerateBlockGPUTaskResult::convert_to_voxel_buffer(to_span(_gpu_generation_results), *_voxels);
	_stage = 2;
}

void GenerateBlockTask::run_cpu_generation() {
	const Vector3i origin_in_voxels = (_position << _lod_index) * _block_size;

	Ref<VoxelGenerator> generator = _stream_dependency->generator;

	VoxelGenerator::VoxelQueryData query_data{ *_voxels, origin_in_voxels, _lod_index };
	const VoxelGenerator::Result result = generator->generate_block(query_data);
	_max_lod_hint = result.max_lod_hint;

	if (_data != nullptr) {
		_data->get_modifiers().apply(query_data.voxel_buffer,
				AABB(query_data.origin_in_voxels, query_data.voxel_buffer.get_size() << _lod_index));
	}
}

void GenerateBlockTask::run_stream_saving_and_finish() {
	if (_stream_dependency->valid) {
		Ref<VoxelStream> stream = _stream_dependency->stream;

		// TODO In some cases we don't want this to run all the time, do we?
		// Like in full load mode, where non-edited blocks remain generated on the fly...
		if (stream.is_valid() && stream->get_save_generator_output()) {
			ZN_PRINT_VERBOSE(
					format("Requesting save of generator output for block {} lod {}", _position, int(_lod_index)));

			// TODO Optimization: `voxels` doesn't actually need to be shared
			std::shared_ptr<VoxelBuffer> voxels_copy = make_shared_instance<VoxelBuffer>();
			_voxels->copy_to(*voxels_copy, true);

			// No instances, generators are not designed to produce them at this stage yet.
			// No priority data, saving doesn't need sorting.

			SaveBlockDataTask *save_task = ZN_NEW(SaveBlockDataTask(
					_volume_id, _position, _lod_index, voxels_copy, _stream_dependency, nullptr, false));

			VoxelEngine::get_singleton().push_async_io_task(save_task);
		}
	}

	_has_run = true;
}

TaskPriority GenerateBlockTask::get_priority() {
	float closest_viewer_distance_sq;
	const TaskPriority p = _priority_dependency.evaluate(
			_lod_index, constants::TASK_PRIORITY_GENERATE_BAND2, &closest_viewer_distance_sq);
	_too_far = _drop_beyond_max_distance && closest_viewer_distance_sq > _priority_dependency.drop_distance_squared;
	return p;
}

bool GenerateBlockTask::is_cancelled() {
	if (_stream_dependency->valid == false) {
		return false;
	}
	if (_cancellation_token.is_valid()) {
		return _cancellation_token.is_cancelled();
	}
	return _too_far; // || stream_dependency->stream->get_fallback_generator().is_null();
}

void GenerateBlockTask::apply_result() {
	bool aborted = true;

	if (VoxelEngine::get_singleton().is_volume_valid(_volume_id)) {
		// TODO Comparing pointer may not be guaranteed
		// The request response must match the dependency it would have been requested with.
		// If it doesn't match, we are no longer interested in the result.
		if (_stream_dependency->valid) {
			Ref<VoxelStream> stream = _stream_dependency->stream;

			VoxelEngine::BlockDataOutput o;
			o.voxels = _voxels;
			o.position = _position;
			o.lod_index = _lod_index;
			o.dropped = !_has_run;
			if (stream.is_valid() && stream->get_save_generator_output()) {
				// We can't consider the block as "generated" since there is no state to tell that once saved,
				// so it has to be considered an edited block
				o.type = VoxelEngine::BlockDataOutput::TYPE_LOADED;
			} else {
				o.type = VoxelEngine::BlockDataOutput::TYPE_GENERATED;
			}
			o.max_lod_hint = _max_lod_hint;
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
