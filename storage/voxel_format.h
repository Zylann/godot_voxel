#ifndef VOXEL_FORMAT_H
#define VOXEL_FORMAT_H

#include "voxel_buffer.h"
#include <array>

namespace zylann::voxel {

struct VoxelFormat {
	VoxelFormat();

	void configure_buffer(VoxelBuffer &vb) const;

	bool operator==(const VoxelFormat &other) const {
		return depths == other.depths;
	}

	struct DepthRange {
		uint32_t min;
		uint32_t max;

		inline bool contains(uint32_t i) const {
			return i >= min && i <= max;
		}
	};

	inline uint64_t get_default_raw_value(const VoxelBuffer::ChannelId channel_id) const {
		return VoxelBuffer::get_default_raw_value(channel_id, depths[channel_id]);
	}

	static DepthRange get_supported_depths(const VoxelBuffer::ChannelId channel_id);
	static uint64_t get_default_sdf_raw_value(const VoxelBuffer::Depth depth);

	std::array<VoxelBuffer::Depth, VoxelBuffer::MAX_CHANNELS> depths;
};

} // namespace zylann::voxel

#endif // VOXEL_FORMAT_H
