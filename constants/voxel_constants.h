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

// 24 should be largely enough.
// With a block size of 32 voxels, and if 1 voxel is 1m large,
// then the largest blocks will span 268,435.456 kilometers, which is roughly 20 times Earth's diameter.
// Using a higher maximum can cause int32 overflows when calculating dimensions. There is no use case for it.
static const unsigned int MAX_LOD = 24;

static const unsigned int MAX_VOLUME_EXTENT = 0x1fffffff;
static const unsigned int MAX_VOLUME_SIZE = 2 * MAX_VOLUME_EXTENT; // 1,073,741,822 voxels

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
