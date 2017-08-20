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

	static VoxelBlock *create(Vector3i bpos, Ref<VoxelBuffer> buffer, unsigned int size);

	void set_mesh(Ref<Mesh> mesh, Ref<World> world);

private:
	VoxelBlock();

	Vector3i _position_in_voxels;

	Ref<Mesh> _mesh;
	RID _mesh_instance;
};

#endif // VOXEL_BLOCK_H
