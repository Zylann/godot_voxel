#include "virtual_rendering_compute_shader.h"
#include "../generators/voxel_generator.h"

namespace zylann::voxel {

#include "render_normalmap_shader_template.h"

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

	std::shared_ptr<ComputeShader> shader = ComputeShader::create_from_glsl(source_text);
	return shader;
}

} // namespace zylann::voxel
