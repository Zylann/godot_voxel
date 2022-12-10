#ifndef VOXEL_COMPUTE_SHADER_PARAMETERS_H
#define VOXEL_COMPUTE_SHADER_PARAMETERS_H

//#include "../util/godot/string.h"
#include "compute_shader_resource.h"
#include <vector>

namespace zylann::voxel {

// Equivalent of "materials" for compute shaders we use in the voxel engine
struct ComputeShaderParameters {
	struct Param {
		unsigned int binding = 0;
		ComputeShaderResource resource;
	};
	std::vector<Param> params;
};

} // namespace zylann::voxel

#endif // VOXEL_COMPUTE_SHADER_PARAMETERS_H
