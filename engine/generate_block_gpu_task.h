#ifndef VOXEL_GENERATE_BLOCK_GPU_TASK_H
#define VOXEL_GENERATE_BLOCK_GPU_TASK_H

#include "../generators/voxel_generator.h"
#include "../util/math/box3i.h"
#include "gpu_storage_buffer_pool.h"
#include "gpu_task_runner.h"

namespace zylann::voxel {

class MeshBlockTask;

class GenerateBlockGPUTask : public IGPUTask {
public:
	~GenerateBlockGPUTask();

	void prepare(GPUTaskContext &ctx) override;
	void collect(GPUTaskContext &ctx) override;

	// Exclusive boxes relative to a VoxelBuffer (not world voxel coordinates)
	std::vector<Box3i> boxes_to_generate;
	// Position of the lower corner of the VoxelBuffer in world voxel coordinates
	Vector3i origin_in_voxels;
	uint8_t lod_index = 0;
	// Task for which the voxel data is for.
	MeshBlockTask *mesh_task = nullptr;

	// Base generator
	std::shared_ptr<ComputeShader> generator_shader;
	std::shared_ptr<ComputeShaderParameters> generator_shader_params;

	// TODO Modifiers

private:
	struct BoxData {
		GPUStorageBuffer params_sb;
		GPUStorageBuffer output_voxel_data_sb;
		Ref<RDUniform> params_uniform;
		Ref<RDUniform> output_voxel_data_uniform;
	};

	std::vector<BoxData> _boxes_data;
	RID _generator_pipeline_rid;
};

} // namespace zylann::voxel

#endif // VOXEL_GENERATE_BLOCK_GPU_TASK_H
