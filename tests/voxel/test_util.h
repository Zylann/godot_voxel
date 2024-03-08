#ifndef VOXEL_TEST_UTIL_H
#define VOXEL_TEST_UTIL_H

namespace zylann::voxel {

class VoxelBuffer;

namespace tests {

bool sd_equals_approx(const VoxelBuffer &vb1, const VoxelBuffer &vb2);

} // namespace tests
} // namespace zylann::voxel

#endif // VOXEL_TEST_UTIL_H
