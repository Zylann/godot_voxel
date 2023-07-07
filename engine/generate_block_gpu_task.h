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

	unsigned int get_required_shared_output_buffer_size() const override;

	void prepare(GPUTaskContext &ctx) override;
	void collect(GPUTaskContext &ctx) override;

	// TODO Not sure if it's worth dealing with sub-boxes. That's only in case of partially-edited meshing blocks...
	// it doesn't sound like a common case.

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
	std::shared_ptr<VoxelGenerator::ShaderOutputs> generator_shader_outputs;

	struct ModifierData {
		RID shader_rid;
		std::shared_ptr<ComputeShaderParameters> params;
	};
	std::vector<ModifierData> modifiers;

private:
	struct BoxData {
		GPUStorageBuffer params_sb;
		Ref<RDUniform> params_uniform;
		Ref<RDUniform> output_uniform;
	};

	std::vector<BoxData> _boxes_data;
	RID _generator_pipeline_rid;
	std::vector<RID> _modifier_pipelines;
};

struct GenerateBlockGPUTaskResult {
	Box3i box;
	VoxelGenerator::ShaderOutput::Type type;
	PackedByteArray bytes;

	static void convert_to_voxel_buffer(Span<GenerateBlockGPUTaskResult> boxes_data, VoxelBufferInternal &dst);
};

} // namespace zylann::voxel

#endif // VOXEL_GENERATE_BLOCK_GPU_TASK_H
