#include "voxel_generator.h"
#include "../constants/voxel_string_names.h"
#include "../engine/compute_shader.h"
#include "../storage/voxel_buffer_gd.h"

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

String VoxelGenerator::get_glsl() const {
	ZN_PRINT_ERROR("Not implemented");
	return "";
}

std::shared_ptr<ComputeShader> VoxelGenerator::get_virtual_rendering_shader() {
	{
		MutexLock mlock(_shader_mutex);
		return _virtual_rendering_shader;
	}
}

#include "../engine/render_normalmap_shader_template.h"

std::shared_ptr<ComputeShader> compile_virtual_rendering_compute_shader(VoxelGenerator &generator) {
	ERR_FAIL_COND_V_MSG(!generator.supports_glsl(), ComputeShader::create_invalid(),
			String("Can't use the provided {0} with compute shaders, it does not support GLSL.")
					.format(varray(VoxelGenerator::get_class_static())));

	const String generator_source = generator.get_glsl();
	ERR_FAIL_COND_V_MSG(generator_source == "", ComputeShader::create_invalid(), "Failed to get shader source code.");

	String source_text;
	{
		source_text += g_render_normalmap_shader_template_0;
		source_text += generator_source;
		source_text += g_render_normalmap_shader_template_1;

		// DEBUG
		print_line("----- Virtual Rendering Shader ------");
		print_line(source_text);
		print_line("-------------------------------------");
	}

	// TODO Pick different name somehow for different generators
	std::shared_ptr<ComputeShader> shader =
			ComputeShader::create_from_glsl(source_text, "zylann.voxel.render_normalmap.gen");
	return shader;
}

void VoxelGenerator::compile_shaders() {
	ERR_FAIL_COND(!supports_glsl());
	ZN_PRINT_VERBOSE("Compiling compute shaders for virtual rendering");
	std::shared_ptr<ComputeShader> vrender_shader = compile_virtual_rendering_compute_shader(*this);
	{
		MutexLock mlock(_shader_mutex);
		_virtual_rendering_shader = vrender_shader;
	}
}

void VoxelGenerator::invalidate_shaders() {
	{
		MutexLock mlock(_shader_mutex);
		_virtual_rendering_shader.reset();
	}
}

void VoxelGenerator::_bind_methods() {
	ClassDB::bind_method(
			D_METHOD("generate_block", "out_buffer", "origin_in_voxels", "lod"), &VoxelGenerator::_b_generate_block);
}

} // namespace zylann::voxel
