#include "rendering_device.h"
#include "../../dstack.h"
#include "rd_sampler_state.h"
#include "rd_shader_source.h"
#include "rd_texture_format.h"
#include "rd_texture_view.h"
#include "rd_uniform.h"

namespace zylann {

void free_rendering_device_rid(RenderingDevice &rd, RID rid) {
	ZN_DSTACK();
#if defined(ZN_GODOT)
	rd.free(rid);
#elif defined(ZN_GODOT_EXTENSION)
	rd.free_rid(rid);
#endif
}

Ref<RDShaderSPIRV> shader_compile_spirv_from_source(RenderingDevice &rd, RDShaderSource &p_source, bool p_allow_cache) {
#if defined(ZN_GODOT)
	// This is a copy of `RenderingDevice::_shader_compile_spirv_from_source` because it's private

	Ref<RDShaderSPIRV> bytecode;
	bytecode.instantiate();
	for (int i = 0; i < RenderingDevice::SHADER_STAGE_MAX; i++) {
		String error;

		RenderingDevice::ShaderStage stage = RenderingDevice::ShaderStage(i);
		String source = p_source.get_stage_source(stage);

		if (!source.is_empty()) {
			Vector<uint8_t> spirv =
					rd.shader_compile_spirv_from_source(stage, source, p_source.get_language(), &error, p_allow_cache);
			bytecode->set_stage_bytecode(stage, spirv);
			bytecode->set_stage_compile_error(stage, error);
		}
	}
	return bytecode;

#elif defined(ZN_GODOT_EXTENSION)
	Ref<RDShaderSource> source_ref(&p_source);
	return rd.shader_compile_spirv_from_source(source_ref, p_allow_cache);
#endif
}

RID shader_create_from_spirv(RenderingDevice &rd, RDShaderSPIRV &p_spirv, String name) {
#if defined(ZN_GODOT)
	// This is a copy of `RenderingDevice::_shader_create_from_spirv` because it's private

	Vector<RenderingDevice::ShaderStageSPIRVData> stage_data;
	for (int i = 0; i < RD::SHADER_STAGE_MAX; i++) {
		RenderingDevice::ShaderStage stage = RenderingDevice::ShaderStage(i);
		RenderingDevice::ShaderStageSPIRVData sd;
		sd.shader_stage = stage;
		String error = p_spirv.get_stage_compile_error(stage);
		ERR_FAIL_COND_V_MSG(!error.is_empty(), RID(),
				"Can't create a shader from an errored bytecode. Check errors in source bytecode.");
		sd.spir_v = p_spirv.get_stage_bytecode(stage);
		if (sd.spir_v.is_empty()) {
			continue;
		}
		stage_data.push_back(sd);
	}
	return rd.shader_create_from_spirv(stage_data, name);

#elif defined(ZN_GODOT_EXTENSION)
	Ref<RDShaderSPIRV> spirv_data_ref(&p_spirv);
	return rd.shader_create_from_spirv(spirv_data_ref, name);
#endif
}

RID texture_create(RenderingDevice &rd, RDTextureFormat &p_format, RDTextureView &p_view,
		const TypedArray<PackedByteArray> &p_data) {
#if defined(ZN_GODOT)
	// This is a partial re-implementation of `RenderingDevice::_texture_create` because it's private

	Vector<Vector<uint8_t>> data;
	for (int i = 0; i < p_data.size(); i++) {
		Vector<uint8_t> byte_slice = p_data[i];
		ERR_FAIL_COND_V(byte_slice.is_empty(), RID());
		data.push_back(byte_slice);
	}

	// Can't access `base` from `RDTextureFormat` because it's private, so I have to re-make it here by hand...
	RenderingDevice::TextureFormat tf;
	tf.width = p_format.get_width();
	tf.height = p_format.get_height();
	tf.depth = p_format.get_depth();
	tf.mipmaps = p_format.get_mipmaps();
	tf.samples = p_format.get_samples();
	tf.array_layers = p_format.get_array_layers();
	tf.texture_type = p_format.get_texture_type();
	tf.usage_bits = p_format.get_usage_bits();
	tf.format = p_format.get_format();

	// Can't access `base` from `RDTextureView` because it's private, so I have to re-make it here by hand...
	RenderingDevice::TextureView tv;
	tv.format_override = p_view.get_format_override();
	tv.swizzle_r = p_view.get_swizzle_r();
	tv.swizzle_g = p_view.get_swizzle_g();
	tv.swizzle_b = p_view.get_swizzle_b();
	tv.swizzle_a = p_view.get_swizzle_a();

	return rd.texture_create(tf, tv, data);

#elif defined(ZN_GODOT_EXTENSION)
	Ref<RDTextureFormat> format_ref(&p_format);
	Ref<RDTextureView> view_ref(&p_view);
	return rd.texture_create(format_ref, view_ref, p_data);
#endif
}

RID uniform_set_create(RenderingDevice &rd, Array uniforms, RID shader, int shader_set) {
#if defined(ZN_GODOT)
	// Can't access the version of that method taking an `Array` because it is private...
	return rd.call(SNAME("uniform_set_create"), uniforms, shader, shader_set);

#elif defined(ZN_GODOT_EXTENSION)
	return rd.uniform_set_create(uniforms, shader, shader_set);
#endif
}

RID sampler_create(RenderingDevice &rd, const RDSamplerState &sampler_state) {
#if defined(ZN_GODOT)
	// Can't access the version of that method taking an `RDSamplerState` object because it is private...

	// return rd.call(SNAME("sampler_create"), sampler_state_ref);

	RenderingDevice::SamplerState ss;
	ss.mag_filter = sampler_state.get_mag_filter();
	ss.min_filter = sampler_state.get_min_filter();
	ss.mip_filter = sampler_state.get_mip_filter();
	ss.repeat_u = sampler_state.get_repeat_u();
	ss.repeat_v = sampler_state.get_repeat_v();
	ss.repeat_w = sampler_state.get_repeat_w();
	ss.lod_bias = sampler_state.get_lod_bias();
	ss.use_anisotropy = sampler_state.get_use_anisotropy();
	ss.anisotropy_max = sampler_state.get_anisotropy_max();
	ss.enable_compare = sampler_state.get_enable_compare();
	ss.compare_op = sampler_state.get_compare_op();
	ss.min_lod = sampler_state.get_min_lod();
	ss.max_lod = sampler_state.get_max_lod();
	ss.border_color = sampler_state.get_border_color();
	ss.unnormalized_uvw = sampler_state.get_unnormalized_uvw();

	return rd.sampler_create(ss);

#elif defined(ZN_GODOT_EXTENSION)
	Ref<RDSamplerState> sampler_state_ref(&sampler_state);
	return rd.sampler_create(sampler_state_ref);
#endif
}

Error update_storage_buffer(RenderingDevice &rd, RID rid, unsigned int offset, unsigned int size,
		const PackedByteArray &pba, unsigned int post_barrier) {
#if defined(ZN_GODOT)
	return rd.buffer_update(rid, offset, size, pba.ptr(), post_barrier);
#elif defined(ZN_GODOT_EXTENSION)
	return rd.buffer_update(rid, offset, size, pba, post_barrier);
#endif
}

} // namespace zylann
