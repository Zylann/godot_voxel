#ifndef VOXEL_COUNTING_H
#define VOXEL_COUNTING_H

#include "../util/containers/std_unordered_map.h"
#include "voxel_buffer.h"
#include <array>

namespace zylann::voxel {

uint32_t count_sdf_lower_than_value(
		const VoxelBuffer &voxels,
		const VoxelBuffer::ChannelId channel_index,
		const float isolevel
);

std::array<uint64_t, 16> count_materials_mixel4(
		const VoxelBuffer &voxels,
		const VoxelBuffer::ChannelId indices_channel_index,
		const VoxelBuffer::ChannelId weights_channel_index
);

std::array<uint64_t, 16> count_materials_mixel4_with_sdf_lower_than_value(
		const VoxelBuffer &voxels,
		const VoxelBuffer::ChannelId sdf_channel_index,
		const VoxelBuffer::ChannelId indices_channel_index,
		const VoxelBuffer::ChannelId weights_channel_index,
		const float isolevel
);

uint32_t count_not_equal_to_buffer(
		const VoxelBuffer &voxels,
		const VoxelBuffer::ChannelId channel,
		const VoxelBuffer &other
);

uint32_t count_equal_to_value(const VoxelBuffer &voxels, const VoxelBuffer::ChannelId channel, const uint32_t value);

uint32_t count_not_equal_to_value(
		const VoxelBuffer &voxels,
		const VoxelBuffer::ChannelId channel,
		const uint32_t value
);

StdUnorderedMap<uint32_t, uint32_t> count_values(const VoxelBuffer &voxels, const VoxelBuffer::ChannelId channel_index);

void count_values_u8_with_sdf_lower_than(
		const VoxelBuffer &voxels,
		const VoxelBuffer::ChannelId sd_channel_index,
		const VoxelBuffer::ChannelId indices_channel_index,
		const float isolevel,
		std::array<uint32_t, 256> &counts
);

} // namespace zylann::voxel

#endif // VOXEL_COUNTING_H
