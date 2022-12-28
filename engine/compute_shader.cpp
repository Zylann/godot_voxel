#include "compute_shader.h"
#include "../util/godot/classes/rd_shader_source.h"
#include "../util/godot/core/array.h" // for `varray` in GDExtension builds
#include "../util/godot/core/print_string.h"
#include "../util/profiling.h"
#include "voxel_engine.h"

namespace zylann::voxel {

ComputeShader::ComputeShader() {}

ComputeShader::ComputeShader(RID p_rid) : _rid(p_rid) {}

ComputeShader::~ComputeShader() {
	clear();
}

void ComputeShader::clear() {
	if (_rid.is_valid()) {
		ZN_PROFILE_SCOPE();
		ZN_ASSERT_RETURN(VoxelEngine::get_singleton().has_rendering_device());
		RenderingDevice &rd = VoxelEngine::get_singleton().get_rendering_device();
		free_rendering_device_rid(rd, _rid);
		_rid = RID();
	}
}

String format_source_code_with_line_numbers(String src) {
	String dst;
	const PackedStringArray lines = src.split("\n", true);
	for (int i = 0; i < lines.size(); ++i) {
		const int line_number = i + 1;
		dst += String::num_int64(line_number);
		if (line_number < 10) {
			dst += "   ";
		} else if (line_number < 100) {
			dst += "  ";
		} else if (line_number < 1000) {
			dst += " ";
		}
		dst += "| ";
		dst += lines[i];
		dst += "\n";
	}
	return dst;
}

void ComputeShader::load_from_glsl(String source_text, String name) {
	ZN_PROFILE_SCOPE();
	clear();

	Ref<RDShaderSource> shader_source;
	shader_source.instantiate();
	shader_source->set_language(RenderingDevice::SHADER_LANGUAGE_GLSL);
	shader_source->set_stage_source(RenderingDevice::SHADER_STAGE_COMPUTE, source_text);

	ZN_ASSERT_RETURN(VoxelEngine::get_singleton().has_rendering_device());
	RenderingDevice &rd = VoxelEngine::get_singleton().get_rendering_device();
	// MutexLock mlock(VoxelEngine::get_singleton().get_rendering_device_mutex());

	Ref<RDShaderSPIRV> shader_spirv = shader_compile_spirv_from_source(rd, **shader_source, false);
	ERR_FAIL_COND(shader_spirv.is_null());

	const String error_message = shader_spirv->get_stage_compile_error(RenderingDevice::SHADER_STAGE_COMPUTE);
	if (error_message != "") {
		ERR_PRINT(String("Failed to compile compute shader '{0}'").format(varray(name)));
		print_line(error_message);

		if (is_verbose_output_enabled()) {
			const String formatted_source_text = format_source_code_with_line_numbers(source_text);
			print_line(formatted_source_text);
		}

		return;
	}

	// TODO What name should I give this shader? Seems it is used for caching
	const RID shader_rid = shader_create_from_spirv(rd, **shader_spirv, name);
	ERR_FAIL_COND(!shader_rid.is_valid());

	_rid = shader_rid;
}

std::shared_ptr<ComputeShader> ComputeShader::create_from_glsl(String source_text, String name) {
	std::shared_ptr<ComputeShader> shader = make_shared_instance<ComputeShader>();
	shader->load_from_glsl(source_text, name);
	return shader;
}

std::shared_ptr<ComputeShader> ComputeShader::create_invalid() {
	return make_shared_instance<ComputeShader>(RID());
}

} // namespace zylann::voxel
