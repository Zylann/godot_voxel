#include "compute_shader_parameters.h"
#include "../../util/godot/classes/rd_uniform.h"
#include "../voxel_engine.h"

namespace zylann::voxel {

void add_uniform_params(const std::vector<ComputeShaderParameter> &params, Array &uniforms) {
	for (const ComputeShaderParameter &p : params) {
		ZN_ASSERT(p.resource != nullptr);
		ZN_ASSERT(p.resource->is_valid());

		Ref<RDUniform> uniform;
		uniform.instantiate();

		uniform->set_binding(p.binding);

		switch (p.resource->get_type()) {
			case ComputeShaderResource::TYPE_TEXTURE_2D:
			case ComputeShaderResource::TYPE_TEXTURE_3D:
				uniform->set_uniform_type(RenderingDevice::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE);
				uniform->add_id(VoxelEngine::get_singleton().get_filtering_sampler());
				break;

			case ComputeShaderResource::TYPE_STORAGE_BUFFER:
				uniform->set_uniform_type(RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
				break;

			default:
				// May add more types if necessary
				ZN_CRASH_MSG("Unhandled type");
				break;
		}

		uniform->add_id(p.resource->get_rid());

		uniforms.append(uniform);
	}
}

} // namespace zylann::voxel
