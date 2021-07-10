#ifndef VOXEL_CONSTANTS_H
#define VOXEL_CONSTANTS_H

#include <core/math/math_defs.h>

namespace VoxelConstants {

// These constants are chosen so you don't accidentally blow up resource usage
static const float MINIMUM_LOD_DISTANCE = 16.f;
static const float MAXIMUM_LOD_DISTANCE = 128.f;

static const unsigned int MIN_BLOCK_SIZE = 16;
static const unsigned int MAX_BLOCK_SIZE = 32;

static const unsigned int MAX_BLOCK_COUNT_PER_REQUEST = 4 * 4 * 4;

static const unsigned int MAX_LOD = 32;
static const unsigned int MAX_VOLUME_EXTENT = 0x1fffffff;
static const unsigned int MAX_VOLUME_SIZE = 2 * MAX_VOLUME_EXTENT; // 1,073,741,822 voxels
static const unsigned int MAIN_THREAD_MESHING_BUDGET_MS = 8;

static const float INV_0x7f = 1.f / 0x7f;
static const float INV_0x7fff = 1.f / 0x7fff;
static const float INV_TAU = 1.f / Math_TAU;

// Below 32 bits, channels are normalized in -1..1, and can represent a limited number of values.
// For storing SDF, we need a range of values that extends beyond that, in particular for better LOD.
// So we can scale it to better fit the resolution.
static const float QUANTIZED_SDF_8_BITS_SCALE = 0.1f;
static const float QUANTIZED_SDF_8_BITS_SCALE_INV = 1.f / 0.1f;
static const float QUANTIZED_SDF_16_BITS_SCALE = 0.002f;
static const float QUANTIZED_SDF_16_BITS_SCALE_INV = 1.f / 0.002f;

static const unsigned int DEFAULT_BLOCK_SIZE_PO2 = 4;

static const float DEFAULT_COLLISION_MARGIN = 0.04f;

} // namespace VoxelConstants

#endif // VOXEL_CONSTANTS_H
