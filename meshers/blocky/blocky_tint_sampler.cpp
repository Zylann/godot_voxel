#include "blocky_tint_sampler.h"
#include "../../storage/voxel_buffer.h"
#include "../../util/errors.h"
#include "../../util/math/color8.h"

namespace zylann::voxel::blocky {

TintSampler TintSampler::create(const VoxelBuffer &p_voxels, const Mode mode) {
	switch (mode) {
		case MODE_NONE:
			break;

		case MODE_RAW: {
			static constexpr VoxelBuffer::ChannelId channel = VoxelBuffer::CHANNEL_COLOR;
			const VoxelBuffer::Depth depth = p_voxels.get_channel_depth(channel);

			switch (depth) {
				case VoxelBuffer::DEPTH_16_BIT:
					return { [](const TintSampler &self, const Vector3i pos) {
								const uint32_t v = self.voxels.get_voxel(pos, channel);
								return Color(Color8::from_u16(v));
							},
							 p_voxels };

				case VoxelBuffer::DEPTH_32_BIT:
					return { [](const TintSampler &self, const Vector3i pos) {
								const uint32_t v = self.voxels.get_voxel(pos, channel);
								return Color(Color8::from_u32(v));
							},
							 p_voxels };

				default:
					ZN_PRINT_ERROR_ONCE("Color channel depth not supported");
					break;
			}
		} break;

		default:
			ZN_PRINT_ERROR_ONCE("Unknown mode");
			break;
	}

	return { nullptr, p_voxels };
}

} // namespace zylann::voxel::blocky
