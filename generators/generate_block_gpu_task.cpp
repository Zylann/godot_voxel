#include "generate_block_gpu_task.h"
#include "../engine/gpu/compute_shader.h"
#include "../engine/gpu/compute_shader_parameters.h"
#include "../engine/voxel_engine.h"
#include "../meshers/mesh_block_task.h"
#include "../modifiers/voxel_modifier.h"
#include "../storage/materials_4i4w.h"
#include "../util/dstack.h"
#include "../util/godot/core/packed_arrays.h"
#include "../util/math/conv.h"
#include "../util/profiling.h"
#include "../util/string/format.h"

#include "../util/godot/classes/rendering_device.h"

namespace zylann::voxel {

GenerateBlockGPUTask::~GenerateBlockGPUTask() {
	if (consumer_task != nullptr) {
		// If we get here, it means the engine got shut down before a mesh task could complete,
		// so we still have ownership on this task and it should be deleted from here.
		ZN_PRINT_VERBOSE("Freeing interrupted consumer task");
		// TODO We may not assume how this task was allocated
		ZN_DELETE(consumer_task);
	}
}

unsigned int GenerateBlockGPUTask::get_required_shared_output_buffer_size() const {
	unsigned int volume = 0;
	for (const Box3i &box : boxes_to_generate) {
		volume += Vector3iUtil::get_volume(box.size);
	}
	// All outputs are floats at the moment...
	return generator_shader_outputs->outputs.size() * volume * sizeof(float);
}

void GenerateBlockGPUTask::prepare(GPUTaskContext &ctx) {
	ZN_PROFILE_SCOPE();
	ZN_DSTACK();

	ZN_ASSERT_RETURN(generator_shader != nullptr);
	ZN_ASSERT_RETURN(generator_shader->is_valid());

	ZN_ASSERT_RETURN(generator_shader_params != nullptr);
	ZN_ASSERT_RETURN(generator_shader_outputs != nullptr);
	ZN_ASSERT_RETURN(generator_shader_outputs->outputs.size() > 0);

	ZN_ASSERT(consumer_task != nullptr);

	ERR_FAIL_COND(boxes_to_generate.size() == 0);

	RenderingDevice &rd = ctx.rendering_device;
	GPUStorageBufferPool &storage_buffer_pool = ctx.storage_buffer_pool;

	// Modifiers only support SDF for now.
	int sd_output_index = -1;
	{
		int i = 0;
		for (const VoxelGenerator::ShaderOutput &output : generator_shader_outputs->outputs) {
			if (output.type == VoxelGenerator::ShaderOutput::TYPE_SDF) {
				sd_output_index = i;
				break;
			}
			++i;
		}
	}

	_boxes_data.resize(boxes_to_generate.size());

	unsigned int out_offset_elements = 0;

	for (unsigned int i = 0; i < _boxes_data.size(); ++i) {
		BoxData &bd = _boxes_data[i];
		const Box3i box = boxes_to_generate[i];
		const Vector3i buffer_resolution = box.size;
		const unsigned int buffer_volume = Vector3iUtil::get_volume(buffer_resolution);

		// Params

		struct Params {
			Vector3f origin_in_voxels;
			float voxel_size;
			Vector3i block_size;
			int output_buffer_start;
		};

		Params params;
		params.origin_in_voxels = to_vec3f((box.position << lod_index) + origin_in_voxels);
		params.voxel_size = 1 << lod_index;
		params.block_size = buffer_resolution;
		params.output_buffer_start = (ctx.shared_output_buffer_begin / sizeof(float)) + out_offset_elements;

		out_offset_elements += buffer_volume * generator_shader_outputs->outputs.size();

		PackedByteArray params_pba;
		zylann::godot::copy_bytes_to(params_pba, params);

		bd.params_sb = storage_buffer_pool.allocate(params_pba);
		ERR_FAIL_COND(bd.params_sb.is_null());

		bd.params_uniform.instantiate();
		bd.params_uniform->set_uniform_type(RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
		bd.params_uniform->add_id(bd.params_sb.rid);

		// Output

		bd.output_uniform.instantiate();
		bd.output_uniform->set_uniform_type(RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
		bd.output_uniform->add_id(ctx.shared_output_buffer_rid);
	}

	// Pipelines
	// Not sure what a pipeline is required for in compute shaders, it seems to be required "just because"

	const RID generator_shader_rid = generator_shader->get_rid();
	_generator_pipeline_rid = rd.compute_pipeline_create(generator_shader_rid);
	ERR_FAIL_COND(!_generator_pipeline_rid.is_valid());

	for (const ModifierData &modifier : modifiers) {
		ERR_FAIL_COND(!modifier.shader_rid.is_valid());
		const RID rid = rd.compute_pipeline_create(modifier.shader_rid);
		ERR_FAIL_COND(!rid.is_valid());
		_modifier_pipelines.push_back(rid);
	}

	// Make compute list

	const int compute_list_id = rd.compute_list_begin();

	// Generate

	for (unsigned int box_index = 0; box_index < _boxes_data.size(); ++box_index) {
		BoxData &bd = _boxes_data[box_index];

		bd.params_uniform->set_binding(0); // Base params
		bd.output_uniform->set_binding(1);

		Array generator_uniforms;
		generator_uniforms.resize(2);
		generator_uniforms[0] = bd.params_uniform;
		generator_uniforms[1] = bd.output_uniform;

		// Additional params
		if (generator_shader_params != nullptr && generator_shader_params->params.size() > 0) {
			add_uniform_params(generator_shader_params->params, generator_uniforms);
		}

		// Note, this internally locks RenderingDeviceVulkan's class mutex. Which means it could perhaps be used outside
		// of the compute list (which already locks the class mutex until it ends). Thankfully, it uses a recursive
		// Mutex (instead of BinaryMutex)
		const RID generator_uniform_set =
				zylann::godot::uniform_set_create(rd, generator_uniforms, generator_shader_rid, 0);

		{
			ZN_PROFILE_SCOPE_NAMED("compute_list_bind_compute_pipeline");
			rd.compute_list_bind_compute_pipeline(compute_list_id, _generator_pipeline_rid);
		}
		{
			ZN_PROFILE_SCOPE_NAMED("compute_list_bind_uniform_set");
			rd.compute_list_bind_uniform_set(compute_list_id, generator_uniform_set, 0);
		}

		const Box3i &box = boxes_to_generate[box_index];

		// Note, the output buffer might not be a multiple of group size, so the shader should avoid writing out of
		// bounds in some invocations.
		const Vector3i groups = math::ceildiv(box.size, Vector3i(4, 4, 4));
		{
			ZN_PROFILE_SCOPE_NAMED("compute_list_dispatch");
			rd.compute_list_dispatch(compute_list_id, groups.x, groups.y, groups.z);
		}
	}

	// TODO We could reduce barriers by dispatching all generators in the batch (instead of just this task), then we do
	// the barrier?
	rd.compute_list_add_barrier(compute_list_id);

	// Apply modifiers in-place

	if (sd_output_index != -1) {
		for (unsigned int box_index = 0; box_index < _boxes_data.size(); ++box_index) {
			BoxData &bd = _boxes_data[box_index];

			const Box3i &box = boxes_to_generate[box_index];
			const Vector3i groups = math::ceildiv(box.size, Vector3i(4, 4, 4));

			Ref<RDUniform> sd_buffer0_uniform = bd.output_uniform;

			for (unsigned int modifier_index = 0; modifier_index < modifiers.size(); ++modifier_index) {
				const ModifierData &modifier_data = modifiers[modifier_index];
				ZN_ASSERT_CONTINUE(modifier_data.shader_rid.is_valid());

				bd.params_uniform->set_binding(0);
				sd_buffer0_uniform->set_binding(1);

				Array modifier_uniforms;
				modifier_uniforms.resize(2);
				modifier_uniforms[0] = bd.params_uniform;
				modifier_uniforms[1] = sd_buffer0_uniform;

				// Extra params
				if (modifier_data.params != nullptr) {
					add_uniform_params(modifier_data.params->params, modifier_uniforms);
				}

				const RID modifier_uniform_set =
						zylann::godot::uniform_set_create(rd, modifier_uniforms, modifier_data.shader_rid, 0);

				const RID pipeline_rid = _modifier_pipelines[modifier_index];
				rd.compute_list_bind_compute_pipeline(compute_list_id, pipeline_rid);
				rd.compute_list_bind_uniform_set(compute_list_id, modifier_uniform_set, 0);

				rd.compute_list_dispatch(compute_list_id, groups.x, groups.y, groups.z);

				rd.compute_list_add_barrier(compute_list_id);
			}
		}
	}

	rd.compute_list_end();
}

namespace {

StdVector<uint8_t> &get_temporary_conversion_memory_tls() {
	static thread_local StdVector<uint8_t> mem;
	return mem;
}

template <typename T>
inline Span<T> get_temporary_conversion_memory_tls(unsigned int count) {
	StdVector<uint8_t> &mem = get_temporary_conversion_memory_tls();
	mem.resize(count * sizeof(T));
	return to_span(mem).reinterpret_cast_to<T>();
}

void convert_gpu_output_sdf(VoxelBuffer &dst, Span<const float> src_data_f, const Box3i &box) {
	ZN_PROFILE_SCOPE();

	const VoxelBuffer::Depth depth = dst.get_channel_depth(VoxelBuffer::CHANNEL_SDF);
	const float sd_scale = VoxelBuffer::get_sdf_quantization_scale(depth);

	switch (depth) {
		case VoxelBuffer::DEPTH_8_BIT: {
			Span<int8_t> sd_data = get_temporary_conversion_memory_tls<int8_t>(src_data_f.size());
			for (unsigned int i = 0; i < src_data_f.size(); ++i) {
				sd_data[i] = snorm_to_s8(sd_scale * src_data_f[i]);
			}
			dst.copy_channel_from(
					sd_data.to_const(), box.size, Vector3i(), box.size, box.position, VoxelBuffer::CHANNEL_SDF);
		} break;

		case VoxelBuffer::DEPTH_16_BIT: {
			Span<int16_t> sd_data = get_temporary_conversion_memory_tls<int16_t>(src_data_f.size());
			for (unsigned int i = 0; i < src_data_f.size(); ++i) {
				sd_data[i] = snorm_to_s16(sd_scale * src_data_f[i]);
			}

			dst.copy_channel_from(
					sd_data.to_const(), box.size, Vector3i(), box.size, box.position, VoxelBuffer::CHANNEL_SDF);
		} break;

		case VoxelBuffer::DEPTH_32_BIT: {
			dst.copy_channel_from(
					src_data_f.to_const(), box.size, Vector3i(), box.size, box.position, VoxelBuffer::CHANNEL_SDF);
		} break;

		case VoxelBuffer::DEPTH_64_BIT: {
			ZN_PRINT_ERROR("64-bit SDF is not supported");
		} break;

		default:
			ZN_PRINT_ERROR("Unhandled depth");
			break;
	}
}

void convert_gpu_output_single_texture(VoxelBuffer &dst, Span<const float> src_data_f, const Box3i &box) {
	ZN_PROFILE_SCOPE();
	const uint16_t encoded_weights = encode_weights_to_packed_u16_lossy(255, 0, 0, 0);
	dst.fill_area(encoded_weights, box.position, box.position + box.size, VoxelBuffer::CHANNEL_WEIGHTS);

	// Little hack: the destination type is smaller than float, so we can convert in place.
	Span<uint16_t> src_data_u16 = get_temporary_conversion_memory_tls<uint16_t>(src_data_f.size());

	for (unsigned int value_index = 0; value_index < src_data_f.size(); ++value_index) {
		const uint8_t index = math::clamp(int(Math::round(src_data_f[value_index])), 0, 15);
		// Make sure other indices are different so the weights associated with them don't override the first
		// index's weight
		const uint8_t other_index = (index == 0 ? 1 : 0);
		const uint16_t encoded_indices = encode_indices_to_packed_u16(index, other_index, other_index, other_index);
		src_data_u16[value_index] = encoded_indices;
	}

	dst.copy_channel_from(
			src_data_u16.to_const(), box.size, Vector3i(), box.size, box.position, VoxelBuffer::CHANNEL_INDICES);
}

template <typename T>
Span<const T> cast_floats(Span<const float> src_data_f, StdVector<uint8_t> &memory) {
	memory.resize(src_data_f.size() * sizeof(T));
	Span<T> dst = to_span(memory).reinterpret_cast_to<T>();
	unsigned int dst_i = 0;
	for (const float src_value : src_data_f) {
		dst[dst_i] = src_value;
		++dst_i;
	}
	return dst;
}

void convert_gpu_output_uint(
		VoxelBuffer &dst, Span<const float> src_data_f, const Box3i &box, VoxelBuffer::ChannelId channel_index) {
	ZN_PROFILE_SCOPE();

	const VoxelBuffer::Depth depth = dst.get_channel_depth(VoxelBuffer::CHANNEL_SDF);
	StdVector<uint8_t> &tls_temp = get_temporary_conversion_memory_tls();

	switch (depth) {
		case VoxelBuffer::DEPTH_8_BIT: {
			dst.copy_channel_from(cast_floats<uint8_t>(src_data_f, tls_temp), box.size, Vector3i(), box.size,
					box.position, channel_index);
		} break;

		case VoxelBuffer::DEPTH_16_BIT: {
			dst.copy_channel_from(cast_floats<uint16_t>(src_data_f, tls_temp), box.size, Vector3i(), box.size,
					box.position, channel_index);
		} break;

		case VoxelBuffer::DEPTH_32_BIT: {
			dst.copy_channel_from(cast_floats<uint32_t>(src_data_f, tls_temp), box.size, Vector3i(), box.size,
					box.position, channel_index);
		} break;

		case VoxelBuffer::DEPTH_64_BIT: {
			dst.copy_channel_from(cast_floats<uint64_t>(src_data_f, tls_temp), box.size, Vector3i(), box.size,
					box.position, channel_index);
		} break;

		default:
			ZN_PRINT_ERROR("Unhandled depth");
			break;
	}
}

} // namespace

void GenerateBlockGPUTaskResult::convert_to_voxel_buffer(VoxelBuffer &dst) {
	// Shaders can only output float arrays for now. Also looks like GLSL does not have 8-bit or 16-bit data types?
	// TODO Should we convert in the compute shader to reduce bandwidth and CPU work?
	Span<const float> src_data_f = _bytes.reinterpret_cast_to<const float>();

	switch (_type) {
		case VoxelGenerator::ShaderOutput::TYPE_SDF:
			convert_gpu_output_sdf(dst, src_data_f, _box);
			break;

		case VoxelGenerator::ShaderOutput::TYPE_SINGLE_TEXTURE:
			convert_gpu_output_single_texture(dst, src_data_f, _box);
			break;

		case VoxelGenerator::ShaderOutput::TYPE_TYPE:
			convert_gpu_output_uint(dst, src_data_f, _box, VoxelBuffer::CHANNEL_TYPE);
			break;

		default:
			ZN_PRINT_ERROR("Unhandled output");
			break;
	}
}

void GenerateBlockGPUTaskResult::convert_to_voxel_buffer(
		Span<GenerateBlockGPUTaskResult> boxes_data, VoxelBuffer &dst) {
	ZN_PROFILE_SCOPE();

	for (GenerateBlockGPUTaskResult &box_data : boxes_data) {
		box_data.convert_to_voxel_buffer(dst);
	}

	dst.compress_uniform_channels();
}

void GenerateBlockGPUTask::collect(GPUTaskContext &ctx) {
	ZN_PROFILE_SCOPE();
	ZN_DSTACK();

	RenderingDevice &rd = ctx.rendering_device;
	GPUStorageBufferPool &storage_buffer_pool = ctx.storage_buffer_pool;

	StdVector<GenerateBlockGPUTaskResult> results;
	results.reserve(_boxes_data.size());

	// Get span for that specific task
	Span<const uint8_t> outputs_bytes = to_span(ctx.downloaded_shared_output_data)
												.sub(ctx.shared_output_buffer_begin, ctx.shared_output_buffer_size);

	unsigned int box_offset = 0;

	for (unsigned int box_index = 0; box_index < _boxes_data.size(); ++box_index) {
		BoxData &bd = _boxes_data[box_index];
		const Box3i box = boxes_to_generate[box_index];

		// Every output is the same size for now
		const unsigned int size_per_output = Vector3iUtil::get_volume(box.size) * sizeof(float);

		for (unsigned int output_index = 0; output_index < generator_shader_outputs->outputs.size(); ++output_index) {
			const VoxelGenerator::ShaderOutput &output_info = generator_shader_outputs->outputs[output_index];

			GenerateBlockGPUTaskResult result(box, output_info.type,
					// Get span for that specific output
					outputs_bytes.sub(box_offset + size_per_output * output_index, size_per_output),
					// Pass reference to the backing buffer to keep it valid until all consumers are done with it
					ctx.downloaded_shared_output_data);

			results.push_back(result);
		}

		box_offset += size_per_output * generator_shader_outputs->outputs.size();

		storage_buffer_pool.recycle(bd.params_sb);
	}

	zylann::godot::free_rendering_device_rid(rd, _generator_pipeline_rid);

	for (RID rid : _modifier_pipelines) {
		zylann::godot::free_rendering_device_rid(rd, rid);
	}

	// We leave conversion to the CPU task, because we have only one thread for GPU work and it only exists for waiting
	// blocking functions, not doing work
	consumer_task->set_gpu_results(std::move(results));

	// Resume meshing task, pass ownership back to the task runner.
	VoxelEngine::get_singleton().push_async_task(consumer_task);
	consumer_task = nullptr;
}

} // namespace zylann::voxel
