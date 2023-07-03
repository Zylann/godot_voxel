#include "generate_block_gpu_task.h"
#include "../util/dstack.h"
#include "../util/godot/funcs.h"
#include "../util/math/conv.h"
#include "../util/profiling.h"
#include "../util/string_funcs.h"
#include "compute_shader.h"
#include "compute_shader_parameters.h"
#include "mesh_block_task.h"
#include "voxel_engine.h"

#include "../util/godot/classes/rendering_device.h"

namespace zylann::voxel {

GenerateBlockGPUTask::~GenerateBlockGPUTask() {
	if (mesh_task != nullptr) {
		// If we get here, it means the engine got shut down before a mesh task could complete,
		// so we still have ownership on this task and it should be deleted from here.
		ZN_PRINT_VERBOSE("Freeing interrupted meshing task");
		memdelete(mesh_task);
	}
}

void GenerateBlockGPUTask::prepare(GPUTaskContext &ctx) {
	ZN_PROFILE_SCOPE();
	ZN_DSTACK();

	ZN_ASSERT_RETURN(generator_shader != nullptr);
	ZN_ASSERT_RETURN(generator_shader->is_valid());

	ZN_ASSERT_RETURN(generator_shader_params != nullptr);
	ZN_ASSERT_RETURN(generator_shader_outputs != nullptr);
	ZN_ASSERT_RETURN(generator_shader_outputs->outputs.size() > 0);

	ZN_ASSERT(mesh_task != nullptr);
	ZN_ASSERT(mesh_task->block_generation_use_gpu);

	ERR_FAIL_COND(boxes_to_generate.size() == 0);

	RenderingDevice &rd = ctx.rendering_device;
	GPUStorageBufferPool &storage_buffer_pool = ctx.storage_buffer_pool;

	_boxes_data.resize(boxes_to_generate.size());

	for (unsigned int i = 0; i < _boxes_data.size(); ++i) {
		BoxData &bd = _boxes_data[i];
		const Box3i box = boxes_to_generate[i];
		const Vector3i buffer_resolution = box.size;

		// Params

		struct Params {
			Vector3f origin_in_voxels;
			float voxel_size;
			Vector3i block_size;
		};

		Params params;
		params.origin_in_voxels = to_vec3f((box.pos << lod_index) + origin_in_voxels);
		params.voxel_size = 1 << lod_index;
		params.block_size = buffer_resolution;

		PackedByteArray params_pba;
		copy_bytes_to(params_pba, params);

		bd.params_sb = storage_buffer_pool.allocate(params_pba);
		ERR_FAIL_COND(bd.params_sb.is_null());

		bd.params_uniform.instantiate();
		bd.params_uniform->set_uniform_type(RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
		bd.params_uniform->add_id(bd.params_sb.rid);

		// Outputs

		bd.outputs.reserve(generator_shader_outputs->outputs.size());

		unsigned int output_index = 0;
		for (const VoxelGenerator::ShaderOutput &output : generator_shader_outputs->outputs) {
			OutputData od;

			// TODO Could we use the same buffer for multiple boxes, writing to different locations?
			od.sb = storage_buffer_pool.allocate(Vector3iUtil::get_volume(buffer_resolution) * sizeof(float));
			ERR_FAIL_COND(od.sb.is_null());

			od.uniform.instantiate();
			od.uniform->set_uniform_type(RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
			od.uniform->add_id(od.sb.rid);

			bd.outputs.push_back(od);

			++output_index;
		}
	}

	// Pipelines
	// Not sure what a pipeline is required for in compute shaders, it seems to be required "just because"

	const RID generator_shader_rid = generator_shader->get_rid();
	_generator_pipeline_rid = rd.compute_pipeline_create(generator_shader_rid);
	ERR_FAIL_COND(!_generator_pipeline_rid.is_valid());

	// Make compute list

	const int compute_list_id = rd.compute_list_begin();

	// Generate

	for (unsigned int box_index = 0; box_index < _boxes_data.size(); ++box_index) {
		BoxData &bd = _boxes_data[box_index];

		bd.params_uniform->set_binding(0);

		Array generator_uniforms;
		generator_uniforms.resize(1);
		generator_uniforms[0] = bd.params_uniform;

		// Params
		if (generator_shader_params != nullptr && generator_shader_params->params.size() > 0) {
			add_uniform_params(generator_shader_params->params, generator_uniforms);
		}

		// Outputs
		for (unsigned int output_index = 0; output_index < bd.outputs.size(); ++output_index) {
			OutputData &od = bd.outputs[output_index];
			const unsigned int binding = generator_shader_outputs->binding_begin_index + output_index;
			od.uniform->set_binding(binding);
			generator_uniforms.append(od.uniform);
		}

		const RID generator_uniform_set = uniform_set_create(rd, generator_uniforms, generator_shader_rid, 0);

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

	// TODO Barrier and apply modifiers

	// rd.compute_list_add_barrier(compute_list_id);

	rd.compute_list_end();
}

static void convert_gpu_output_sdf(VoxelBufferInternal &dst, Span<float> src_data_f, const Box3i &box) {
	ZN_PROFILE_SCOPE();

	const unsigned int data_volume = src_data_f.size();
	const VoxelBufferInternal::Depth depth = dst.get_channel_depth(VoxelBufferInternal::CHANNEL_SDF);
	const float sd_scale = VoxelBufferInternal::get_sdf_quantization_scale(depth);

	switch (dst.get_channel_depth(VoxelBufferInternal::CHANNEL_SDF)) {
		case VoxelBufferInternal::DEPTH_8_BIT: {
			// Little hack: the destination type is smaller than float, so we can convert in place.
			Span<int8_t> sd_data = src_data_f.reinterpret_cast_to<int8_t>();
			for (unsigned int i = 0; i < src_data_f.size(); ++i) {
				sd_data[i] = snorm_to_s8(sd_scale * src_data_f[i]);
			}
			dst.copy_from(Span<const int8_t>(sd_data).sub(0, data_volume), box.size, Vector3i(), box.size, box.pos,
					VoxelBufferInternal::CHANNEL_SDF);
		} break;

		case VoxelBufferInternal::DEPTH_16_BIT: {
			Span<int16_t> sd_data = src_data_f.reinterpret_cast_to<int16_t>();
			for (unsigned int i = 0; i < src_data_f.size(); ++i) {
				sd_data[i] = snorm_to_s16(sd_scale * src_data_f[i]);
			}
			dst.copy_from(Span<const int16_t>(sd_data).sub(0, data_volume), box.size, Vector3i(), box.size, box.pos,
					VoxelBufferInternal::CHANNEL_SDF);
		} break;

		case VoxelBufferInternal::DEPTH_32_BIT: {
			dst.copy_from(Span<const float>(src_data_f).sub(0, data_volume), box.size, Vector3i(), box.size, box.pos,
					VoxelBufferInternal::CHANNEL_SDF);
		} break;

		case VoxelBufferInternal::DEPTH_64_BIT: {
			ZN_PRINT_ERROR("64-bit SDF is not supported");
		} break;

		default:
			ZN_PRINT_ERROR("Unhandled depth");
			break;
	}
}

static void convert_gpu_output_single_texture(VoxelBufferInternal &dst, Span<float> src_data_f, const Box3i &box) {
	ZN_PROFILE_SCOPE();
	const uint16_t encoded_weights = encode_weights_to_packed_u16_lossy(255, 0, 0, 0);
	dst.fill_area(encoded_weights, box.pos, box.pos + box.size, VoxelBufferInternal::CHANNEL_WEIGHTS);

	// Little hack: the destination type is smaller than float, so we can convert in place.
	Span<uint16_t> src_data_u16 = src_data_f.reinterpret_cast_to<uint16_t>();

	for (unsigned int value_index = 0; value_index < src_data_f.size(); ++value_index) {
		const uint8_t index = math::clamp(int(Math::round(src_data_f[value_index])), 0, 15);
		// Make sure other indices are different so the weights associated with them don't override the first
		// index's weight
		const uint8_t other_index = (index == 0 ? 1 : 0);
		const uint16_t encoded_indices = encode_indices_to_packed_u16(index, other_index, other_index, other_index);
		src_data_u16[value_index] = encoded_indices;
	}

	dst.copy_from(Span<const uint16_t>(src_data_u16).sub(0, src_data_f.size()), box.size, Vector3i(), box.size, box.pos,
			VoxelBufferInternal::CHANNEL_INDICES);
}

struct VoxelGeneratorGPUOutputDataForBox {
	Box3i box;
	VoxelGenerator::ShaderOutput::Type type;
	PackedByteArray bytes;
};

static void convert_gpu_output_data(Span<VoxelGeneratorGPUOutputDataForBox> boxes_data, VoxelBufferInternal &dst) {
	ZN_PROFILE_SCOPE();

	for (VoxelGeneratorGPUOutputDataForBox &box_data : boxes_data) {
		// Shaders can only output float arrays for now. Also looks like GLSL does not have 8-bit or 16-bit data types?
		// TODO Should we convert in the compute shader to reduce bandwidth and CPU work?
		Span<float> src_data_f =
				Span<uint8_t>(box_data.bytes.ptrw(), box_data.bytes.size()).reinterpret_cast_to<float>();

		switch (box_data.type) {
			case VoxelGenerator::ShaderOutput::TYPE_SDF:
				convert_gpu_output_sdf(dst, src_data_f, box_data.box);
				break;

			case VoxelGenerator::ShaderOutput::TYPE_SINGLE_TEXTURE:
				convert_gpu_output_single_texture(dst, src_data_f, box_data.box);
				break;

			default:
				ZN_PRINT_ERROR("Unhandled output");
				break;
		}
	}

	// TODO Clip SDF before doing this? Would accelerate empty mesh skipping
	dst.compress_uniform_channels();
}

void GenerateBlockGPUTask::collect(GPUTaskContext &ctx) {
	ZN_PROFILE_SCOPE();
	ZN_DSTACK();

	RenderingDevice &rd = ctx.rendering_device;
	GPUStorageBufferPool &storage_buffer_pool = ctx.storage_buffer_pool;

	VoxelBufferInternal &dst = mesh_task->voxels;
	const VoxelBufferInternal::Depth sd_depth = dst.get_channel_depth(VoxelBufferInternal::CHANNEL_SDF);
	const float sd_scale = VoxelBufferInternal::get_sdf_quantization_scale(sd_depth);

	std::vector<VoxelGeneratorGPUOutputDataForBox> outputs_data;
	outputs_data.reserve(_boxes_data.size());

	for (unsigned int box_index = 0; box_index < _boxes_data.size(); ++box_index) {
		BoxData &bd = _boxes_data[box_index];
		const Box3i box = boxes_to_generate[box_index];

		const unsigned int data_size = Vector3iUtil::get_volume(box.size) * sizeof(float);

		for (unsigned int output_index = 0; output_index < bd.outputs.size(); ++output_index) {
			const OutputData &od = bd.outputs[output_index];
			PackedByteArray voxel_data;
			{
				ZN_PROFILE_SCOPE_NAMED("buffer_get_data");
				voxel_data = rd.buffer_get_data(od.sb.rid, 0, data_size);
			}
			const VoxelGenerator::ShaderOutput &output_info = generator_shader_outputs->outputs[output_index];
			outputs_data.push_back(VoxelGeneratorGPUOutputDataForBox{ box, output_info.type, voxel_data });

			storage_buffer_pool.recycle(od.sb);
		}

		storage_buffer_pool.recycle(bd.params_sb);
	}

	free_rendering_device_rid(rd, _generator_pipeline_rid);

	// TODO Move this back to the meshing task, the GPU thread must not do such work
	convert_gpu_output_data(to_span(outputs_data), dst);

	// Resume meshing task, pass ownership back to the task runner
	mesh_task->stage = MeshBlockTask::STAGE_BUILD_MESH;
	VoxelEngine::get_singleton().push_async_task(mesh_task);
	mesh_task = nullptr;
}

} // namespace zylann::voxel
