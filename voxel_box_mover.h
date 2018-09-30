#ifndef VOXEL_BOX_MOVER_H
#define VOXEL_BOX_MOVER_H

#include "voxel_terrain.h"
#include <core/math/aabb.h>

// Helper to get simple AABB physics
class VoxelBoxMover : public Reference {
	GDCLASS(VoxelBoxMover, Reference)
public:
	Vector3 get_motion(Vector3 pos, Vector3 motion, AABB aabb, VoxelTerrain *terrain);

protected:
	Vector3 _get_motion_binding(Vector3 pos, Vector3 motion, AABB aabb, Node *terrain_node);

	static void _bind_methods();
};

#endif // VOXEL_BOX_MOVER_H
