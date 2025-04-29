#ifndef VOXEL_PROCESSOR_H
#define VOXEL_PROCESSOR_H

#include "../util/godot/classes/resource.h"

namespace zylann::voxel {

class VoxelBuffer;

class VoxelProcessor : public Resource {
	GDCLASS(VoxelProcessor, Resource)
public:
	virtual void process_block(VoxelBuffer &voxels) {}

	virtual uint32_t get_input_channels_mask() const {
		return 0;
	}

	virtual uint32_t get_output_channels_mask() const {
		return 0;
	}

private:
	static void _bind_methods() {}
};

} // namespace zylann::voxel

#endif // VOXEL_PROCESSOR_H
