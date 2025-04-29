#include "voxel_processor_sdf_to_blocky.h"
#include "../storage/voxel_buffer.h"

namespace zylann::voxel {

template <typename TSd, typename TModelID>
void sdf_to_blocky(
		VoxelBuffer &voxels,
		const VoxelBuffer::ChannelId src_channel_id,
		const VoxelBuffer::ChannelId dst_channel_id
) {
	Span<const TSd> src;
	ZN_ASSERT_RETURN(voxels.get_channel_data_read_only<TSd>(src_channel_id, src));
	Span<TModelID> dst;
	ZN_ASSERT_RETURN(voxels.get_channel_data<TModelID>(dst_channel_id, dst));
	for (unsigned int i = 0; i < src.size(); ++i) {
		const TSd sd = src[i];
		dst[i] = sd <= 0 ? 1 : 0;
	}
}

template <typename TSd>
void sdf_to_blocky_dispatch_blocky(
		VoxelBuffer &voxels,
		const VoxelBuffer::ChannelId src_channel_id,
		const VoxelBuffer::ChannelId dst_channel_id
) {
	switch (voxels.get_channel_depth(dst_channel_id)) {
		case VoxelBuffer::DEPTH_8_BIT:
			// TODO
			break;
		case VoxelBuffer::DEPTH_16_BIT:
			sdf_to_blocky<TSd, uint16_t>(voxels, src_channel_id, dst_channel_id);
			break;
		default:
			break;
	}
}

void VoxelProcessorSDFToBlocky::process_block(VoxelBuffer &voxels) {
	const VoxelBuffer::ChannelId src_channel_id = VoxelBuffer::CHANNEL_SDF;
	const VoxelBuffer::ChannelId dst_channel_id = VoxelBuffer::CHANNEL_TYPE;

	if (voxels.get_channel_compression(src_channel_id) == VoxelBuffer::COMPRESSION_UNIFORM) {
		const float sd = voxels.get_voxel_f(Vector3i(), src_channel_id);
		voxels.fill(sd <= 0.f ? 1 : 0, dst_channel_id);
		return;
	}

	if (voxels.get_channel_compression(dst_channel_id) == VoxelBuffer::COMPRESSION_UNIFORM) {
		voxels.decompress_channel(dst_channel_id);
	}

	switch (voxels.get_channel_depth(src_channel_id)) {
		case VoxelBuffer::DEPTH_8_BIT: {
			// TODO
		} break;

		case VoxelBuffer::DEPTH_16_BIT: {
			sdf_to_blocky_dispatch_blocky<int16_t>(voxels, src_channel_id, dst_channel_id);
		} break;

		case VoxelBuffer::DEPTH_32_BIT: {
			// TODO
		} break;

		default:
			break;
	}
}

uint32_t VoxelProcessorSDFToBlocky::get_input_channels_mask() const {
	return (1 << VoxelBuffer::CHANNEL_SDF);
}

uint32_t VoxelProcessorSDFToBlocky::get_output_channels_mask() const {
	return (1 << VoxelBuffer::CHANNEL_TYPE);
}

} // namespace zylann::voxel
