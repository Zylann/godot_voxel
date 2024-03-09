#ifndef VOXEL_COMPUTE_SHADER_PARAMETERS_H
#define VOXEL_COMPUTE_SHADER_PARAMETERS_H

#include "../../util/containers/std_vector.h"
#include "compute_shader_resource.h"
#include <memory>

namespace zylann::voxel {

struct ComputeShaderParameter {
	unsigned int binding = 0;
	std::shared_ptr<ComputeShaderResource> resource;
};

// Equivalent of "materials" for compute shaders we use in the voxel engine
struct ComputeShaderParameters {
	StdVector<ComputeShaderParameter> params;
};

void add_uniform_params(const StdVector<ComputeShaderParameter> &params, Array &uniforms);

} // namespace zylann::voxel

#endif // VOXEL_COMPUTE_SHADER_PARAMETERS_H
