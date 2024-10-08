#include "test_util.h"
#include "../../storage/voxel_buffer.h"

namespace zylann::voxel::tests {

bool sd_equals_approx(const VoxelBuffer &vb1, const VoxelBuffer &vb2) {
	const VoxelBuffer::ChannelId channel = VoxelBuffer::CHANNEL_SDF;
	const VoxelBuffer::Depth depth = vb1.get_channel_depth(channel);
	// const float error_margin = 1.1f * VoxelBuffer::get_sdf_quantization_scale(depth);
	// There can be a small difference due to scaling operations, so instead of an exact equality, we check approximate
	// equality.
	Vector3i pos;
	for (pos.y = 0; pos.y < vb1.get_size().y; ++pos.y) {
		for (pos.z = 0; pos.z < vb1.get_size().z; ++pos.z) {
			for (pos.x = 0; pos.x < vb1.get_size().x; ++pos.x) {
				switch (depth) {
					case VoxelBuffer::DEPTH_8_BIT: {
						const int sd1 = int8_t(vb1.get_voxel(pos, channel));
						const int sd2 = int8_t(vb2.get_voxel(pos, channel));
						if (Math::abs(sd1 - sd2) > 1) {
							return false;
						}
					} break;
					case VoxelBuffer::DEPTH_16_BIT: {
						const int sd1 = int16_t(vb1.get_voxel(pos, channel));
						const int sd2 = int16_t(vb2.get_voxel(pos, channel));
						if (Math::abs(sd1 - sd2) > 1) {
							return false;
						}
					} break;
					case VoxelBuffer::DEPTH_32_BIT:
					case VoxelBuffer::DEPTH_64_BIT: {
						const float sd1 = vb1.get_voxel_f(pos, channel);
						const float sd2 = vb2.get_voxel_f(pos, channel);
						if (!Math::is_equal_approx(sd1, sd2)) {
							return false;
						}
					} break;
					default:
						ZN_CRASH_MSG("Unhandled depth");
						break;
				}
			}
		}
	}
	return true;
}

} // namespace zylann::voxel::tests
