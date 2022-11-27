#ifndef VOXEL_VIRTUAL_RENDERING_COMPUTE_SHADER_H
#define VOXEL_VIRTUAL_RENDERING_COMPUTE_SHADER_H

#include "compute_shader.h"

namespace zylann::voxel {

class VoxelGenerator;

std::shared_ptr<ComputeShader> compile_virtual_rendering_compute_shader(VoxelGenerator &generator);

} // namespace zylann::voxel

#endif // VOXEL_VIRTUAL_RENDERING_COMPUTE_SHADER_H
