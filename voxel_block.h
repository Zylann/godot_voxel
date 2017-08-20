#ifndef VOXEL_BLOCK_H
#define VOXEL_BLOCK_H

#include "voxel_buffer.h"

#include <scene/3d/mesh_instance.h>
#include <scene/3d/physics_body.h>

// Internal structure holding a reference to mesh visuals, physics and a block of voxel data.
// TODO Voxel data should be separated from this
class VoxelBlock {
public:
	Ref<VoxelBuffer> voxels; // SIZE*SIZE*SIZE voxels
	Vector3i pos;
	NodePath mesh_instance_path;

	static VoxelBlock *create(Vector3i bpos, Ref<VoxelBuffer> buffer, unsigned int size);

	MeshInstance *get_mesh_instance(const Node &root);

private:
	VoxelBlock();
};

#endif // VOXEL_BLOCK_H
