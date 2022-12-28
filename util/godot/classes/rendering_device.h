#ifndef ZN_GODOT_RENDERING_DEVICE_H
#define ZN_GODOT_RENDERING_DEVICE_H

#if defined(ZN_GODOT)
#include <servers/rendering/rendering_device.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/rendering_device.hpp>
using namespace godot;
#endif

#include "../../macros.h"
#include "rd_shader_spirv.h"

ZN_GODOT_FORWARD_DECLARE(class RDShaderSource)

namespace zylann {

void free_rendering_device_rid(RenderingDevice &rd, RID rid);

// TODO GDX: For some reason, the API exposed to scripts and extensions is not exposed to modules...
// This forces me to copy implementations to keep my code the same in both module and extension targets

Ref<RDShaderSPIRV> shader_compile_spirv_from_source(RenderingDevice &rd, RDShaderSource &p_source, bool p_allow_cache);
RID shader_create_from_spirv(RenderingDevice &rd, RDShaderSPIRV &p_spirv, String name = "");
RID texture_create(RenderingDevice &rd, RDTextureFormat &p_format, RDTextureView &p_view,
		const TypedArray<PackedByteArray> &p_data);
RID uniform_set_create(RenderingDevice &rd, Array uniforms, RID shader, int shader_set);
RID sampler_create(RenderingDevice &rd, const RDSamplerState &sampler_state);
Error update_storage_buffer(RenderingDevice &rd, RID rid, unsigned int offset, unsigned int size,
		const PackedByteArray &pba, unsigned int post_barrier = RenderingDevice::BARRIER_MASK_ALL_BARRIERS);

} // namespace zylann

#endif // ZN_GODOT_RENDERING_DEVICE_H
