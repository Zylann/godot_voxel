#ifndef VOXEL_CONSTANTS_H
#define VOXEL_CONSTANTS_H

namespace VoxelConstants {

static const float MINIMUM_LOD_SPLIT_SCALE = 2.f;
static const float MAXIMUM_LOD_SPLIT_SCALE = 5.f;
static const unsigned int MAX_LOD = 32;
static const unsigned int MAX_VOLUME_EXTENT = 0x1fffffff;
static const unsigned int MAX_VOLUME_SIZE = 2 * MAX_VOLUME_EXTENT; // 1,073,741,822 voxels
static const unsigned int MAIN_THREAD_MESHING_BUDGET_MS = 8;

} // namespace VoxelConstants

#endif // VOXEL_CONSTANTS_H
