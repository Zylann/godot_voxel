#ifndef VOXEL_CONSTANTS_H
#define VOXEL_CONSTANTS_H

#include "../util/math/constants.h"
#include <cstdint>

namespace zylann::voxel::constants {

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

static const int MAX_VOLUME_EXTENT = 0x1fffffff;
static const int MAX_VOLUME_SIZE = 2 * MAX_VOLUME_EXTENT; // 1,073,741,822 voxels

static const float INV_0x7f = 1.f / 0x7f;
static const float INV_0x7fff = 1.f / 0x7fff;
static const float INV_TAU = 1.f / math::TAU_32;
static const float SQRT3 = 1.73205080757;

// Below 32 bits, channels are normalized in -1..1, and can represent a limited number of values.
// For storing SDF, we need a range of values that extends beyond that, in particular for better LOD.
// So we can scale it to better fit the resolution. These scales were chosen arbitrarily, but they should work well for
// the corresponding precisions.
static const float QUANTIZED_SDF_8_BITS_SCALE = 0.1f;
static const float QUANTIZED_SDF_8_BITS_SCALE_INV = 1.f / 0.1f;
static const float QUANTIZED_SDF_16_BITS_SCALE = 0.002f;
static const float QUANTIZED_SDF_16_BITS_SCALE_INV = 1.f / 0.002f;

static const unsigned int DEFAULT_BLOCK_SIZE_PO2 = 4;

static const float DEFAULT_COLLISION_MARGIN = 0.04f;

// By default, tasks are sorted first by the value of band2.
// When equal, they are sorted by band1, which usually depends on LOD.
// When equal, they are sorted by band0, which depends on distance from viewer (when relevant).
// band3 takes precedence over band2 but isn't used much for now.
static const uint8_t TASK_PRIORITY_MESH_BAND2 = 10;
static const uint8_t TASK_PRIORITY_GENERATE_BAND2 = 10;
static const uint8_t TASK_PRIORITY_LOAD_BAND2 = 10;
static const uint8_t TASK_PRIORITY_SAVE_BAND2 = 9;
static const uint8_t TASK_PRIORITY_VIRTUAL_TEXTURES_BAND2 = 8; // After meshes

static const uint8_t TASK_PRIORITY_BAND3_DEFAULT = 10;

} // namespace zylann::voxel::constants

#endif // VOXEL_CONSTANTS_H
