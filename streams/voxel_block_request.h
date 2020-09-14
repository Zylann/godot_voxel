#ifndef VOXEL_BLOCK_REQUEST_H
#define VOXEL_BLOCK_REQUEST_H

#include "../math/vector3i.h"
#include "../storage/voxel_buffer.h"

// TODO Rename VoxelStreamBlockRequest
struct VoxelBlockRequest {
	Ref<VoxelBuffer> voxel_buffer;
	Vector3i origin_in_voxels;
	int lod;
};

#endif // VOXEL_BLOCK_REQUEST_H
