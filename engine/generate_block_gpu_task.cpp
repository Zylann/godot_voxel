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

	ZN_ASSERT(mesh_task != nullptr);
	ZN_ASSERT(mesh_task->block_generation_use_gpu);

	ZN_ASSERT(generator_shader != nullptr);
	ZN_ASSERT(generator_shader->is_valid());

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

		// Output voxel data

		// TODO Could we use the same buffer for multiple boxes, writing to different locations?
		bd.output_voxel_data_sb =
				storage_buffer_pool.allocate(Vector3iUtil::get_volume(buffer_resolution) * sizeof(float));
		ERR_FAIL_COND(bd.output_voxel_data_sb.is_null());

		bd.output_voxel_data_uniform.instantiate();
		bd.output_voxel_data_uniform->set_uniform_type(RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
		bd.output_voxel_data_uniform->add_id(bd.output_voxel_data_sb.rid);
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
		bd.output_voxel_data_uniform->set_binding(1);

		Array generator_uniforms;
		generator_uniforms.resize(2);
		generator_uniforms[0] = bd.params_uniform;
		generator_uniforms[1] = bd.output_voxel_data_uniform;

		// Extra params
		if (generator_shader_params != nullptr && generator_shader_params->params.size() > 0) {
			add_uniform_params(generator_shader_params->params, generator_uniforms);
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

void GenerateBlockGPUTask::collect(GPUTaskContext &ctx) {
	ZN_PROFILE_SCOPE();
	ZN_DSTACK();

	RenderingDevice &rd = ctx.rendering_device;
	GPUStorageBufferPool &storage_buffer_pool = ctx.storage_buffer_pool;

	VoxelBufferInternal &dst = mesh_task->voxels;
	const VoxelBufferInternal::Depth sd_depth = dst.get_channel_depth(VoxelBufferInternal::CHANNEL_SDF);
	const float sd_scale = VoxelBufferInternal::get_sdf_quantization_scale(sd_depth);

	for (unsigned int box_index = 0; box_index < _boxes_data.size(); ++box_index) {
		ZN_PROFILE_SCOPE_NAMED("Box");
		BoxData &bd = _boxes_data[box_index];
		const Box3i box = boxes_to_generate[box_index];

		const unsigned int data_volume = Vector3iUtil::get_volume(box.size);
		const unsigned int data_size = data_volume * sizeof(float);
		PackedByteArray voxel_data;
		{
			ZN_PROFILE_SCOPE_NAMED("buffer_get_data");
			voxel_data = rd.buffer_get_data(bd.output_voxel_data_sb.rid, 0, data_size);
		}

		// TODO Should we convert in the compute shader to reduce bandwidth?
		Span<float> sd_data_f = Span<uint8_t>(voxel_data.ptrw(), voxel_data.size()).reinterpret_cast_to<float>();

		switch (dst.get_channel_depth(VoxelBufferInternal::CHANNEL_SDF)) {
			case VoxelBufferInternal::DEPTH_8_BIT: {
				// Little hack: the destination type is smaller than float, so we can convert in place.
				Span<int8_t> sd_data = sd_data_f.reinterpret_cast_to<int8_t>();
				for (unsigned int i = 0; i < sd_data_f.size(); ++i) {
					sd_data[i] = snorm_to_s8(sd_scale * sd_data_f[i]);
				}
				dst.copy_from(Span<const int8_t>(sd_data).sub(0, data_volume), box.size, Vector3i(), box.size, box.pos,
						VoxelBufferInternal::CHANNEL_SDF);
			} break;

			case VoxelBufferInternal::DEPTH_16_BIT: {
				ZN_PROFILE_SCOPE_NAMED("Convert and copy");
				Span<int16_t> sd_data = sd_data_f.reinterpret_cast_to<int16_t>();
				for (unsigned int i = 0; i < sd_data_f.size(); ++i) {
					sd_data[i] = snorm_to_s16(sd_scale * sd_data_f[i]);
				}
				dst.copy_from(Span<const int16_t>(sd_data).sub(0, data_volume), box.size, Vector3i(), box.size, box.pos,
						VoxelBufferInternal::CHANNEL_SDF);
			} break;

			case VoxelBufferInternal::DEPTH_32_BIT: {
				dst.copy_from(Span<const float>(sd_data_f).sub(0, data_volume), box.size, Vector3i(), box.size, box.pos,
						VoxelBufferInternal::CHANNEL_SDF);
			} break;

			case VoxelBufferInternal::DEPTH_64_BIT: {
				ZN_PRINT_ERROR("64-bit SDF is not supported");
			} break;

			default:
				ZN_PRINT_ERROR("Unhandled depth");
				break;
		}

		storage_buffer_pool.recycle(bd.params_sb);
		storage_buffer_pool.recycle(bd.output_voxel_data_sb);
	}

	free_rendering_device_rid(rd, _generator_pipeline_rid);

	// Resume meshing task, pass ownership back to the task runner
	mesh_task->stage = MeshBlockTask::STAGE_BUILD_MESH;
	VoxelEngine::get_singleton().push_async_task(mesh_task);
	mesh_task = nullptr;
}

} // namespace zylann::voxel
