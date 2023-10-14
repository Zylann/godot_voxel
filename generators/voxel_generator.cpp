#include "voxel_generator.h"
#include "../constants/voxel_string_names.h"
#include "../engine/generate_block_task.h"
#include "../engine/gpu/compute_shader.h"
#include "../engine/gpu/compute_shader_parameters.h"
#include "../shaders/shaders.h"
#include "../storage/voxel_buffer_gd.h"
#include "../util/godot/core/array.h" // for `varray` in GDExtension builds
#include "../util/profiling.h"

namespace zylann::voxel {

VoxelGenerator::VoxelGenerator() {}

VoxelGenerator::Result VoxelGenerator::generate_block(VoxelQueryData &input) {
	return Result();
}

IThreadedTask *VoxelGenerator::create_block_task(const BlockTaskParams &params) const {
	// Default generic task
	return ZN_NEW(GenerateBlockTask(params));
}

int VoxelGenerator::get_used_channels_mask() const {
	return 0;
}

VoxelSingleValue VoxelGenerator::generate_single(Vector3i pos, unsigned int channel) {
	VoxelSingleValue v;
	v.i = 0;
	ZN_ASSERT_RETURN_V(channel >= 0 && channel < VoxelBufferInternal::MAX_CHANNELS, v);
	// Default slow implementation
	// TODO Optimize: a small part of the slowness is caused by the allocator.
	// It is not a good use of `VoxelMemoryPool` for such a small size called so often.
	// Instead it would be faster if it was a thread-local using the default allocator.
	VoxelBufferInternal buffer;
	buffer.create(1, 1, 1);
	VoxelQueryData q{ buffer, pos, 0 };
	generate_block(q);
	if (channel == VoxelBufferInternal::CHANNEL_SDF) {
		v.f = buffer.get_voxel_f(0, 0, 0, channel);
	} else {
		v.i = buffer.get_voxel(0, 0, 0, channel);
	}
	return v;
}

void VoxelGenerator::generate_series(Span<const float> positions_x, Span<const float> positions_y,
		Span<const float> positions_z, unsigned int channel, Span<float> out_values, Vector3f min_pos,
		Vector3f max_pos) {
	ZN_PRINT_ERROR("Not implemented");
}

void VoxelGenerator::_b_generate_block(Ref<gd::VoxelBuffer> out_buffer, Vector3 origin_in_voxels, int lod) {
	ERR_FAIL_COND(lod < 0);
	ERR_FAIL_COND(lod >= int(constants::MAX_LOD));
	ERR_FAIL_COND(out_buffer.is_null());
	VoxelQueryData q = { out_buffer->get_buffer(), origin_in_voxels, uint8_t(lod) };
	generate_block(q);
}

bool VoxelGenerator::get_shader_source(ShaderSourceData &out_params) const {
	ZN_PRINT_ERROR("Not implemented");
	return false;
}

std::shared_ptr<ComputeShader> VoxelGenerator::get_detail_rendering_shader() {
	{
		MutexLock mlock(_shader_mutex);
		return _detail_rendering_shader;
	}
}

std::shared_ptr<ComputeShaderParameters> VoxelGenerator::get_detail_rendering_shader_parameters() {
	{
		MutexLock mlock(_shader_mutex);
		return _detail_rendering_shader_parameters;
	}
}

std::shared_ptr<ComputeShader> VoxelGenerator::get_block_rendering_shader() {
	{
		MutexLock mlock(_shader_mutex);
		return _block_rendering_shader;
	}
}

std::shared_ptr<ComputeShaderParameters> VoxelGenerator::get_block_rendering_shader_parameters() {
	{
		MutexLock mlock(_shader_mutex);
		return _block_rendering_shader_parameters;
	}
}

std::shared_ptr<VoxelGenerator::ShaderOutputs> VoxelGenerator::get_block_rendering_shader_outputs() {
	{
		MutexLock mlock(_shader_mutex);
		return _block_rendering_shader_outputs;
	}
}

static void append_generator_parameter_uniforms(String &source_text, ComputeShaderParameters &out_params,
		VoxelGenerator::ShaderSourceData &shader_data, unsigned int bindings_start) {
	for (unsigned int i = 0; i < shader_data.parameters.size(); ++i) {
		VoxelGenerator::ShaderParameter &p = shader_data.parameters[i];
		const unsigned int binding = bindings_start + i;
		ZN_ASSERT(p.resource.get_type() == ComputeShaderResource::TYPE_TEXTURE_2D);
		source_text +=
				String("layout (set = 0, binding = {0}) uniform sampler2D {1};\n").format(varray(binding, p.name));
		std::shared_ptr<ComputeShaderResource> res = make_unique_instance<ComputeShaderResource>();
		*res = std::move(p.resource);
		out_params.params.push_back(ComputeShaderParameter{ binding, res });
	}
	source_text += "\n";
}

std::shared_ptr<ComputeShader> compile_detail_rendering_compute_shader(
		VoxelGenerator &generator, ComputeShaderParameters &out_params) {
	ZN_PROFILE_SCOPE();
	ERR_FAIL_COND_V_MSG(!generator.supports_shaders(), ComputeShader::create_invalid(),
			String("Can't use the provided {0} with compute shaders, it does not support GLSL.")
					.format(varray(VoxelGenerator::get_class_static())));

	VoxelGenerator::ShaderSourceData shader_data;
	ERR_FAIL_COND_V_MSG(!generator.get_shader_source(shader_data), ComputeShader::create_invalid(),
			"Failed to get shader source code.");

	String source_text;
	// We are only sure here what binding it's going to be, we can't do it earlier
	const unsigned int generator_uniform_binding_start = 3;
	{
		// Header
		source_text += g_detail_generator_shader_template_0;

		append_generator_parameter_uniforms(source_text, out_params, shader_data, generator_uniform_binding_start);

		// Generator code
		source_text += shader_data.glsl;

		// Generate wrapper to use only one output, and adapt to the function name expected by the detail rendering
		// template
		{
			source_text += "float get_sd(vec3 pos) {\n";
			int sdf_output_index = -1;
			for (unsigned int output_index = 0; output_index < shader_data.outputs.size(); ++output_index) {
				const VoxelGenerator::ShaderOutput &output = shader_data.outputs[output_index];
				if (output.type == VoxelGenerator::ShaderOutput::TYPE_SDF) {
					sdf_output_index = output_index;
				}
				source_text += String("\tfloat v{0};\n").format(varray(output_index));
			}
			ERR_FAIL_COND_V_MSG(sdf_output_index == -1, ComputeShader::create_invalid(),
					"Can't generate detail generator shader, SDF output not found");
			// Call the generator shader function
			source_text += "\tgenerate(pos";
			for (unsigned int output_index = 0; output_index < shader_data.outputs.size(); ++output_index) {
				source_text += String(", v{0}").format(varray(output_index));
			}
			source_text += ");\n";
			source_text += String("\treturn v{0};\n}\n").format(varray(sdf_output_index));
		}

		// Footer
		source_text += g_detail_generator_shader_template_1;
	}

	// TODO Pick different name somehow for different generators
	std::shared_ptr<ComputeShader> shader =
			ComputeShader::create_from_glsl(source_text, "zylann.voxel.detail_generator.gen");

	return shader;
}

std::shared_ptr<ComputeShader> compile_block_rendering_compute_shader(
		VoxelGenerator &generator, ComputeShaderParameters &out_params, VoxelGenerator::ShaderOutputs &outputs) {
	ZN_PROFILE_SCOPE();
	ERR_FAIL_COND_V_MSG(!generator.supports_shaders(), ComputeShader::create_invalid(),
			String("Can't use the provided {0} with compute shaders, it does not support GLSL.")
					.format(varray(VoxelGenerator::get_class_static())));

	VoxelGenerator::ShaderSourceData shader_data;
	ERR_FAIL_COND_V_MSG(!generator.get_shader_source(shader_data), ComputeShader::create_invalid(),
			"Failed to get shader source code.");

	String source_text;
	const unsigned int generator_uniform_binding_start = 2;

	source_text += g_block_generator_shader_template_0;

	append_generator_parameter_uniforms(source_text, out_params, shader_data, generator_uniform_binding_start);

	for (unsigned int output_index = 0; output_index < shader_data.outputs.size(); ++output_index) {
		const VoxelGenerator::ShaderOutput &output = shader_data.outputs[output_index];
		outputs.outputs.push_back(output);
	}

	// Generator code
	source_text += shader_data.glsl;

	// Header of main()
	source_text += g_block_generator_shader_template_1;

	// Call generator function
	{
		source_text += "\tgenerate(wpos";
		for (unsigned int output_index = 0; output_index < shader_data.outputs.size(); ++output_index) {
			// TODO Perhaps we should be able to pack outputs instead of always using floats?
			// TODO Maybe interleaved output would be more performant due to data locality?
			source_text += String(", u_out.values[out_index + volume * {0}]").format(varray(output_index));
		}
		source_text += ");\n";
	}

	// Footer of main()
	source_text += g_block_generator_shader_template_2;

	// TODO Pick different name somehow for different generators
	std::shared_ptr<ComputeShader> shader =
			ComputeShader::create_from_glsl(source_text, "zylann.voxel.block_generator.gen");

	return shader;
}

void VoxelGenerator::compile_shaders() {
	ZN_PROFILE_SCOPE();
	ERR_FAIL_COND(!supports_shaders());
	ZN_PRINT_VERBOSE("Compiling compute shaders for virtual rendering");

	std::shared_ptr<ComputeShaderParameters> detail_params = make_shared_instance<ComputeShaderParameters>();
	std::shared_ptr<ComputeShader> detail_render_shader =
			compile_detail_rendering_compute_shader(*this, *detail_params);

	std::shared_ptr<ComputeShaderParameters> block_params = make_shared_instance<ComputeShaderParameters>();
	std::shared_ptr<ShaderOutputs> block_outputs = make_shared_instance<ShaderOutputs>();
	std::shared_ptr<ComputeShader> block_render_shader =
			compile_block_rendering_compute_shader(*this, *block_params, *block_outputs);

	{
		MutexLock mlock(_shader_mutex);

		_detail_rendering_shader = detail_render_shader;
		_detail_rendering_shader_parameters = detail_params;

		_block_rendering_shader = block_render_shader;
		_block_rendering_shader_parameters = block_params;
		_block_rendering_shader_outputs = block_outputs;
	}
}

void VoxelGenerator::invalidate_shaders() {
	{
		MutexLock mlock(_shader_mutex);

		_detail_rendering_shader.reset();
		_detail_rendering_shader_parameters.reset();

		_block_rendering_shader.reset();
		_block_rendering_shader_parameters.reset();
		_block_rendering_shader_outputs.reset();
	}
}

bool VoxelGenerator::generate_broad_block(VoxelQueryData &input) {
	// By default, generators don't support this separately and just do it inside `generate_block`.
	// However if a generator supports GPU, it is recommended to implement it.
	return false;
}

void VoxelGenerator::process_viewer_diff(ViewerID viewer_id, Box3i p_requested_box, Box3i p_prev_requested_box) {
	// Optionally implemented in subclasses
}

void VoxelGenerator::clear_cache() {
	// Optionally implemented in subclasses
}

void VoxelGenerator::_bind_methods() {
	ClassDB::bind_method(
			D_METHOD("generate_block", "out_buffer", "origin_in_voxels", "lod"), &VoxelGenerator::_b_generate_block);
}

} // namespace zylann::voxel
