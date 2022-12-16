#ifndef VOXEL_COMPUTE_SHADER_PARAMETERS_H
#define VOXEL_COMPUTE_SHADER_PARAMETERS_H

#include "compute_shader_resource.h"
#include <memory>
#include <vector>

namespace zylann::voxel {

struct ComputeShaderParameter {
	unsigned int binding = 0;
	std::shared_ptr<ComputeShaderResource> resource;
};

// Equivalent of "materials" for compute shaders we use in the voxel engine
struct ComputeShaderParameters {
	std::vector<ComputeShaderParameter> params;
};

} // namespace zylann::voxel

#endif // VOXEL_COMPUTE_SHADER_PARAMETERS_H
