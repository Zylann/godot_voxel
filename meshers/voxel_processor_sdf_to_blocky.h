#ifndef VOXEL_PROCESSOR_SDF_TO_BLOCKY_H
#define VOXEL_PROCESSOR_SDF_TO_BLOCKY_H

#include "voxel_processor.h"

namespace zylann::voxel {

class VoxelProcessorSDFToBlocky : public VoxelProcessor {
	GDCLASS(VoxelProcessorSDFToBlocky, VoxelProcessor)
public:
	void process_block(VoxelBuffer &voxels) override;
	uint32_t get_input_channels_mask() const override;
	uint32_t get_output_channels_mask() const override;

private:
	static void _bind_methods() {}
};

} // namespace zylann::voxel

#endif // VOXEL_PROCESSOR_SDF_TO_BLOCKY_H
