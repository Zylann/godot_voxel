#include "voxel_generator.h"
#include "../constants/voxel_string_names.h"
#include "../engine/compute_shader.h"
#include "../engine/compute_shader_parameters.h"
#include "../shaders/shaders.h"
#include "../storage/voxel_buffer_gd.h"
#include "../util/godot/core/array.h" // for `varray` in GDExtension builds

namespace zylann::voxel {

VoxelGenerator::VoxelGenerator() {}

VoxelGenerator::Result VoxelGenerator::generate_block(VoxelQueryData &input) {
	return Result();
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

std::shared_ptr<ComputeShader> VoxelGenerator::get_virtual_rendering_shader() {
	{
		MutexLock mlock(_shader_mutex);
		return _virtual_rendering_shader;
	}
}

std::shared_ptr<ComputeShaderParameters> VoxelGenerator::get_virtual_rendering_shader_parameters() {
	{
		MutexLock mlock(_shader_mutex);
		return _virtual_rendering_shader_parameters;
	}
}

std::shared_ptr<ComputeShader> compile_virtual_rendering_compute_shader(
		VoxelGenerator &generator, ComputeShaderParameters &out_params) {
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
		source_text += g_detail_generator_shader_template_0;

		for (unsigned int i = 0; i < shader_data.parameters.size(); ++i) {
			VoxelGenerator::ShaderParameter &p = shader_data.parameters[i];
			const unsigned int binding = generator_uniform_binding_start + i;
			ZN_ASSERT(p.resource.get_type() == ComputeShaderResource::TYPE_TEXTURE_2D);
			source_text +=
					String("layout (set = 0, binding = {0}) uniform sampler2D {1};\n").format(varray(binding, p.name));
			std::shared_ptr<ComputeShaderResource> res = make_unique_instance<ComputeShaderResource>();
			*res = std::move(p.resource);
			out_params.params.push_back(ComputeShaderParameter{ binding, res });
		}
		source_text += "\n";

		source_text += shader_data.glsl;
		source_text += g_detail_generator_shader_template_1;
	}

	// TODO Pick different name somehow for different generators
	std::shared_ptr<ComputeShader> shader =
			ComputeShader::create_from_glsl(source_text, "zylann.voxel.detail_generator.gen");

	return shader;
}

void VoxelGenerator::compile_shaders() {
	ERR_FAIL_COND(!supports_shaders());
	ZN_PRINT_VERBOSE("Compiling compute shaders for virtual rendering");

	std::shared_ptr<ComputeShaderParameters> params = make_shared_instance<ComputeShaderParameters>();
	std::shared_ptr<ComputeShader> vrender_shader = compile_virtual_rendering_compute_shader(*this, *params);

	{
		MutexLock mlock(_shader_mutex);
		_virtual_rendering_shader = vrender_shader;

		_virtual_rendering_shader_parameters = params;
	}
}

void VoxelGenerator::invalidate_shaders() {
	{
		MutexLock mlock(_shader_mutex);
		_virtual_rendering_shader.reset();
		_virtual_rendering_shader_parameters.reset();
	}
}

void VoxelGenerator::_bind_methods() {
	ClassDB::bind_method(
			D_METHOD("generate_block", "out_buffer", "origin_in_voxels", "lod"), &VoxelGenerator::_b_generate_block);
}

} // namespace zylann::voxel
