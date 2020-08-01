#ifndef VOXEL_BOX_MOVER_H
#define VOXEL_BOX_MOVER_H

#include "voxel_terrain.h"
#include <core/math/aabb.h>

// Helper to get simple AABB physics
class VoxelBoxMover : public Reference {
	GDCLASS(VoxelBoxMover, Reference)
public:
	Vector3 get_motion(Vector3 pos, Vector3 motion, AABB aabb, VoxelTerrain *terrain);

	void set_collision_mask(uint32_t mask);
	inline uint32_t get_collision_mask() const { return _collision_mask; }

private:
	Vector3 _b_get_motion(Vector3 pos, Vector3 motion, AABB aabb, Node *terrain_node);

	static void _bind_methods();

	std::vector<AABB> _colliding_boxes;
	uint32_t _collision_mask = 0xffffffff; // Everything
};

#endif // VOXEL_BOX_MOVER_H
