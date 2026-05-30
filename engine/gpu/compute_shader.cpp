#include "compute_shader.h"
#include "../../util/godot/classes/directory.h"
#include "../../util/godot/classes/engine.h"
#include "../../util/godot/classes/file_access.h"
#include "../../util/godot/classes/rd_shader_source.h"
#include "../../util/godot/classes/rendering_server.h"
#include "../../util/godot/core/array.h" // for `varray` in GDExtension builds
#include "../../util/godot/core/packed_arrays.h"
#include "../../util/godot/core/print_string.h"
#include "../../util/godot/classes/project_settings.h"
#include "../../util/io/log.h"
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

namespace {

String get_voxel_compute_shader_cache_base_dir() {
	String base_dir;
#if defined(ZN_GODOT)
	base_dir = Engine::get_singleton()->get_shader_cache_path();
#endif
	if (base_dir.is_empty()) {
		base_dir = "user://shader_cache";
	}
	return base_dir;
}

String get_compute_shader_cache_dir(const String &name) {
	return get_voxel_compute_shader_cache_base_dir()
			.path_join(name.validate_filename());
}

String get_compute_shader_binary_cache_path(const String &source_hash, const String &device_cache_uuid, const String &name) {
	
	return get_compute_shader_cache_dir(name)
			.path_join(vformat("%s.%s.bin.cache", source_hash, device_cache_uuid));
}

PackedByteArray load_compute_shader_binary_from_cache(const String &cache_file_path, const String &shader_name) {
	if (!zylann::godot::file_exists(cache_file_path)) {
		return PackedByteArray();
	}

	Error open_error;
	Ref<FileAccess> file = zylann::godot::open_file(cache_file_path, FileAccess::READ, open_error);
	if (file.is_null()) {
		ZN_PRINT_VERBOSE(format("Could not open VoxelRD compute shader binary cache file {} for {}", cache_file_path, shader_name));
		return PackedByteArray();
	}

	const uint64_t length = file->get_length();
	if (length == 0) {
		ZN_PRINT_VERBOSE(format("VoxelRD compute shader binary cache file is empty for {}", shader_name));
		return PackedByteArray();
	}

	PackedByteArray bytes;
	bytes.resize(length);
	const uint64_t read_count = zylann::godot::get_buffer(**file, Span<uint8_t>(bytes.ptrw(), bytes.size()));
	if (read_count != length) {
		ZN_PRINT_VERBOSE(format("Could not read full VoxelRD compute shader binary cache file for {}", shader_name));
		return PackedByteArray();
	}

	return bytes;
}

void save_compute_shader_binary_to_cache(
		const String &cache_file_path,
		const PackedByteArray &shader_binary,
		const String &shader_name
) {
	if (shader_binary.is_empty()) {
		return;
	}

	const Error dir_error = DirAccess::make_dir_recursive_absolute(cache_file_path.get_base_dir());
	if (dir_error != OK) {
		ZN_PRINT_WARNING(format("Could not create voxel compute shader binary cache directory for {}", shader_name));
		return;
	}

	Error open_error;
	Ref<FileAccess> file = zylann::godot::open_file(cache_file_path, FileAccess::WRITE, open_error);
	if (file.is_null()) {
		ZN_PRINT_WARNING(format("Could not open VoxelRD compute shader binary cache file {} for writing", cache_file_path));
		return;
	}

	zylann::godot::store_buffer(**file, to_span(shader_binary));
}

} // namespace

Ref<RDShaderSPIRV> compile_compute_shader_spirv_from_glsl(RenderingDevice &rd, String source_text, String name) {
	// Refactored since both cache and non cache paths need this
	
	Ref<RDShaderSource> shader_source;
	shader_source.instantiate();
	shader_source->set_language(RenderingDevice::SHADER_LANGUAGE_GLSL);
	shader_source->set_stage_source(RenderingDevice::SHADER_STAGE_COMPUTE, source_text);

	Ref<RDShaderSPIRV> shader_spirv = zylann::godot::shader_compile_spirv_from_source(rd, **shader_source, false);
	ERR_FAIL_COND_V(shader_spirv.is_null(), Ref<RDShaderSPIRV>());

	String error_message = shader_spirv->get_stage_compile_error(RenderingDevice::SHADER_STAGE_COMPUTE);
	if (error_message != "") {
		ERR_PRINT(String("Failed to compile compute shader '{0}'").format(varray(name)));
		::print_line(error_message);

		if (is_verbose_output_enabled()) {
			const String formatted_source_text = format_source_code_with_line_numbers(source_text);
			::print_line(formatted_source_text);
		}

		return Ref<RDShaderSPIRV>();
	}

	return shader_spirv;
}

RID load_compute_shader_from_glsl(RenderingDevice &rd, String source_text, String name) {
	ZN_PRINT_VERBOSE(format("Creating VoxelRD compute shader {}", name));
	// For debugging
	// {
	// 	Ref<FileAccess> f = FileAccess::open("debug_" + name + ".txt", FileAccess::WRITE);
	// 	ZN_ASSERT(f.is_valid());
	// 	f->store_string(source_text);
	// }

	// ZN_ASSERT_RETURN_MSG(
	// 		VoxelEngine::get_singleton().has_rendering_device(),
	// 		format("Can't create compute shader \"{}\". Maybe the selected renderer doesn't support it? ({})",
	// 			   name,
	// 			   zylann::godot::get_current_rendering_method())
	// );
	// MutexLock mlock(VoxelEngine::get_singleton().get_rendering_device_mutex());

	Ref<RDShaderSPIRV> shader_spirv = compile_compute_shader_spirv_from_glsl(rd, source_text, name);
	ERR_FAIL_COND_V(shader_spirv.is_null(), RID());

	RID shader_rid = zylann::godot::shader_create_from_spirv(rd, **shader_spirv, name);
	ERR_FAIL_COND_V(!shader_rid.is_valid(), RID());

	return shader_rid;
}

RID load_compute_shader_from_glsl_cached(RenderingDevice &rd, String source_text, String name) {
	ZN_PRINT_VERBOSE(format("Creating VoxelRD compute shader {}", name));

	const String source_hash = source_text.sha256_text();
	const String binary_cache_file_path =
			get_compute_shader_binary_cache_path(source_hash, rd.get_device_pipeline_cache_uuid(), name);

	PackedByteArray shader_binary = load_compute_shader_binary_from_cache(binary_cache_file_path, name);
	if (!shader_binary.is_empty()) {
		RID shader_rid = rd.shader_create_from_bytecode(shader_binary);
		if (shader_rid.is_valid()) {
			ZN_PRINT_VERBOSE(format("Loaded VoxelRD compute shader {} from binary cache", name));
			return shader_rid;
		}
		ZN_PRINT_VERBOSE(format("VoxelRD compute shader binary cache rejected by RenderingDevice for {}", name));
	}

	Ref<RDShaderSPIRV> shader_spirv = compile_compute_shader_spirv_from_glsl(rd, source_text, name);
	ERR_FAIL_COND_V(shader_spirv.is_null(), RID());

	shader_binary = zylann::godot::shader_compile_binary_from_spirv(rd, **shader_spirv, name);
	ERR_FAIL_COND_V(shader_binary.is_empty(), RID());

	save_compute_shader_binary_to_cache(binary_cache_file_path, shader_binary, name);

	RID shader_rid = rd.shader_create_from_bytecode(shader_binary);
	if (shader_rid.is_valid()) {
		return shader_rid;
	}

	ZN_PRINT_VERBOSE(format("VoxelRD compute shader binary rejected by RenderingDevice for {}, falling back to SPIR-V", name));

	shader_rid = zylann::godot::shader_create_from_spirv(rd, **shader_spirv, name);
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

	ZN_ASSERT(ProjectSettings::get_singleton() != nullptr);
	const bool shader_cache_enabled = ProjectSettings::get_singleton()->get("voxel/shaders/shader_cache/enabled");
	if (shader_cache_enabled) {
		rid = load_compute_shader_from_glsl_cached(rd, source_text, name);
	} else {
		rid = load_compute_shader_from_glsl(rd, source_text, name);
	}
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
