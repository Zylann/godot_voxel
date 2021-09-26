#ifndef VOXEL_BLOCK_REQUEST_H
#define VOXEL_BLOCK_REQUEST_H

#include "../storage/voxel_buffer.h"
#include "../util/math/vector3i.h"
#include "instance_data.h"
#include <memory>

// TODO Rename VoxelStreamBlockRequest
struct VoxelBlockRequest {
	VoxelBufferInternal &voxel_buffer;
	Vector3i origin_in_voxels;
	int lod;
};

struct VoxelStreamInstanceDataRequest {
	std::unique_ptr<VoxelInstanceBlockData> data;
	Vector3i position;
	uint8_t lod;
};

#endif // VOXEL_BLOCK_REQUEST_H
