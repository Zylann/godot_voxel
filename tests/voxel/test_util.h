#ifndef VOXEL_TEST_UTIL_H
#define VOXEL_TEST_UTIL_H

namespace zylann::voxel {

class VoxelBufferInternal;

namespace tests {

bool sd_equals_approx(const VoxelBufferInternal &vb1, const VoxelBufferInternal &vb2);

} // namespace tests
} // namespace zylann::voxel

#endif // VOXEL_TEST_UTIL_H
