#include "compute_shader.h"
#include "../../util/godot/classes/rd_shader_source.h"
#include "../../util/godot/classes/rendering_server.h"
#include "../../util/godot/core/array.h" // for `varray` in GDExtension builds
#include "../../util/godot/core/print_string.h"
#include "../../util/profiling.h"
#include "../../util/string/format.h"
#include "../voxel_engine.h"

namespace zylann::voxel {

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

RID load_compute_shader_from_glsl(RenderingDevice &rd, String source_text, String name) {
	ZN_PRINT_VERBOSE(format("Creating VoxelRD compute shader {}", name));
	// For debugging
	// {
	// 	Ref<FileAccess> f = FileAccess::open("debug_" + name + ".txt", FileAccess::WRITE);
	// 	ZN_ASSERT(f.is_valid());
	// 	f->store_string(source_text);
	// }

	Ref<RDShaderSource> shader_source;
	shader_source.instantiate();
	shader_source->set_language(RenderingDevice::SHADER_LANGUAGE_GLSL);
	shader_source->set_stage_source(RenderingDevice::SHADER_STAGE_COMPUTE, source_text);

	// ZN_ASSERT_RETURN_MSG(
	// 		VoxelEngine::get_singleton().has_rendering_device(),
	// 		format("Can't create compute shader \"{}\". Maybe the selected renderer doesn't support it? ({})",
	// 			   name,
	// 			   zylann::godot::get_current_rendering_method())
	// );
	// MutexLock mlock(VoxelEngine::get_singleton().get_rendering_device_mutex());

	Ref<RDShaderSPIRV> shader_spirv = zylann::godot::shader_compile_spirv_from_source(rd, **shader_source, false);
	ERR_FAIL_COND_V(shader_spirv.is_null(), RID());

	const String error_message = shader_spirv->get_stage_compile_error(RenderingDevice::SHADER_STAGE_COMPUTE);
	if (error_message != "") {
		ERR_PRINT(String("Failed to compile compute shader '{0}'").format(varray(name)));
		::print_line(error_message);

		if (is_verbose_output_enabled()) {
			const String formatted_source_text = format_source_code_with_line_numbers(source_text);
			::print_line(formatted_source_text);
		}

		return RID();
	}

	// TODO What name should I give this shader? Seems it is used for caching
	const RID shader_rid = zylann::godot::shader_create_from_spirv(rd, **shader_spirv, name);
	ERR_FAIL_COND_V(!shader_rid.is_valid(), RID());

	return shader_rid;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ComputeShaderInternal::clear(RenderingDevice &rd) {
	if (rid.is_valid()) {
		ZN_PROFILE_SCOPE();
#if DEBUG_ENABLED
		ZN_PRINT_VERBOSE(format("Freeing VoxelRD compute shader {}", debug_name));
#else
		ZN_PRINT_VERBOSE("Freeing VoxelRD compute shader");
#endif
		zylann::godot::free_rendering_device_rid(rd, rid);
		rid = RID();
	}
}

void ComputeShaderInternal::load_from_glsl(RenderingDevice &rd, String source_text, String name) {
	ZN_PROFILE_SCOPE();
	clear(rd);
	rid = load_compute_shader_from_glsl(rd, source_text, name);
#if DEBUG_ENABLED
	debug_name = name;
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ComputeShader::~ComputeShader() {
	ComputeShaderInternal internal = _internal;
	VoxelEngine::get_singleton().push_gpu_task_f([internal](GPUTaskContext &ctx) { //
		// *sigh*
		ComputeShaderInternal internal2 = internal;
		internal2.clear(ctx.rendering_device);
	});
}

std::shared_ptr<ComputeShader> ComputeShaderFactory::create_from_glsl(String source_text, String name) {
	std::shared_ptr<ComputeShader> shader = make_shared_instance<ComputeShader>();
	VoxelEngine::get_singleton().push_gpu_task_f([shader, source_text, name](GPUTaskContext &ctx) {
		shader->_internal.load_from_glsl(ctx.rendering_device, source_text, name);
	});
	return shader;
}

std::shared_ptr<ComputeShader> ComputeShaderFactory::create_invalid() {
	return make_shared_instance<ComputeShader>();
}

RID ComputeShader::get_rid() const {
	// TODO Assert that we are on the GPU tasks thread
	return _internal.rid;
}

// std::shared_ptr<ComputeShader> ComputeShader::create_invalid() {
// 	return make_shared_instance<ComputeShader>(RID());
// }

} // namespace zylann::voxel
