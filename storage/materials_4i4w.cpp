#include "materials_4i4w.h"
#include "voxel_buffer.h"

namespace zylann::voxel {

void debug_check_texture_indices_packed_u16(const VoxelBuffer &voxels) {
	for (int z = 0; z < voxels.get_size().z; ++z) {
		for (int x = 0; x < voxels.get_size().x; ++x) {
			for (int y = 0; y < voxels.get_size().y; ++y) {
				uint16_t pi = voxels.get_voxel(x, y, z, VoxelBuffer::CHANNEL_INDICES);
				FixedArray<uint8_t, 4> indices = decode_indices_from_packed_u16(pi);
				debug_check_texture_indices(indices);
			}
		}
	}
}

} // namespace zylann::voxel
