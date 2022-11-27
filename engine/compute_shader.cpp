#include "compute_shader.h"
#include "voxel_engine.h"

namespace zylann::voxel {

ComputeShader::ComputeShader(RID p_rid) : _rid(p_rid) {}

ComputeShader::~ComputeShader() {
	if (_rid.is_valid()) {
		RenderingDevice *rd = VoxelEngine::get_singleton().get_rendering_device();
		ERR_FAIL_COND(rd == nullptr);
		MutexLock mlock(VoxelEngine::get_singleton().get_rendering_device_mutex());
		free_rendering_device_rid(*rd, _rid);
	}
}

std::shared_ptr<ComputeShader> ComputeShader::create_from_glsl(String source_text) {
	Ref<RDShaderSource> shader_source;
	shader_source.instantiate();
	shader_source->set_language(RenderingDevice::SHADER_LANGUAGE_GLSL);
	shader_source->set_stage_source(RenderingDevice::SHADER_STAGE_COMPUTE, source_text);

	RenderingDevice *rd = VoxelEngine::get_singleton().get_rendering_device();
	ERR_FAIL_COND_V(rd == nullptr, ComputeShader::create_invalid());
	// MutexLock mlock(VoxelEngine::get_singleton().get_rendering_device_mutex());

	Ref<RDShaderSPIRV> shader_spirv = shader_compile_spirv_from_source(*rd, **shader_source, false);
	ERR_FAIL_COND_V(shader_spirv.is_null(), ComputeShader::create_invalid());

	const String error_message = shader_spirv->get_stage_compile_error(RenderingDevice::SHADER_STAGE_COMPUTE);
	if (error_message != "") {
		ERR_PRINT("Failed to compile compute shader");
		print_line(error_message);
		return ComputeShader::create_invalid();
	}

	// TODO What name should I give this shader? Seems it is used for caching
	const RID shader_rid = shader_create_from_spirv(*rd, **shader_spirv, "voxel_virtual_render");
	ERR_FAIL_COND_V(shader_rid.is_null(), ComputeShader::create_invalid());

	return make_shared_instance<ComputeShader>(shader_rid);
}

std::shared_ptr<ComputeShader> ComputeShader::create_invalid() {
	return make_shared_instance<ComputeShader>(RID());
}

} // namespace zylann::voxel
