#include "voxel_format.h"
#include "mixel4.h"

namespace zylann::voxel {

VoxelFormat::VoxelFormat() {
	depths[VoxelBuffer::CHANNEL_TYPE] = VoxelBuffer::DEFAULT_TYPE_CHANNEL_DEPTH;
	depths[VoxelBuffer::CHANNEL_SDF] = VoxelBuffer::DEFAULT_SDF_CHANNEL_DEPTH;
	depths[VoxelBuffer::CHANNEL_COLOR] = VoxelBuffer::DEFAULT_CHANNEL_DEPTH;
	depths[VoxelBuffer::CHANNEL_INDICES] = VoxelBuffer::DEFAULT_INDICES_CHANNEL_DEPTH;
	depths[VoxelBuffer::CHANNEL_WEIGHTS] = VoxelBuffer::DEFAULT_WEIGHTS_CHANNEL_DEPTH;
	depths[VoxelBuffer::CHANNEL_DATA5] = VoxelBuffer::DEFAULT_CHANNEL_DEPTH;
	depths[VoxelBuffer::CHANNEL_DATA6] = VoxelBuffer::DEFAULT_CHANNEL_DEPTH;
	depths[VoxelBuffer::CHANNEL_DATA7] = VoxelBuffer::DEFAULT_CHANNEL_DEPTH;
}

void VoxelFormat::configure_buffer(VoxelBuffer &vb) const {
	// Clear keeping size
	if (vb.get_size() == Vector3i()) {
		vb.clear(this);
	} else {
		vb.create(vb.get_size(), this);
	}
}

VoxelFormat::DepthRange VoxelFormat::get_supported_depths(const VoxelBuffer::ChannelId channel_id) {
	switch (channel_id) {
		case VoxelBuffer::CHANNEL_TYPE:
			return { 1, 2 };
		case VoxelBuffer::CHANNEL_SDF:
			return { 1, 4 };
		case VoxelBuffer::CHANNEL_COLOR:
			return { 1, 4 };
		case VoxelBuffer::CHANNEL_INDICES:
			return { 1, 2 };
		case VoxelBuffer::CHANNEL_WEIGHTS:
			return { 2, 2 };
		case VoxelBuffer::CHANNEL_DATA5:
		case VoxelBuffer::CHANNEL_DATA6:
		case VoxelBuffer::CHANNEL_DATA7:
			return { 1, 4 };
		default:
			ZN_PRINT_ERROR("Unknown channel");
			return { 1, 1 };
	}
}

} // namespace zylann::voxel
