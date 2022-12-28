#ifndef VOXEL_BOX_MOVER_H
#define VOXEL_BOX_MOVER_H

#include "../../util/godot/classes/ref_counted.h"
#include "../../util/macros.h"

ZN_GODOT_FORWARD_DECLARE(class Node);

namespace zylann::voxel {

class VoxelTerrain;

// Helper to get simple AABB physics
class VoxelBoxMover : public RefCounted {
	GDCLASS(VoxelBoxMover, RefCounted)
public:
	Vector3 get_motion(Vector3 pos, Vector3 motion, AABB aabb, VoxelTerrain &terrain);

	void set_collision_mask(uint32_t mask);
	inline uint32_t get_collision_mask() const {
		return _collision_mask;
	}

	void set_step_climbing_enabled(bool enable);
	bool is_step_climbing_enabled() const;

	void set_max_step_height(float height);
	float get_max_step_height() const;

	bool has_stepped_up() const;

private:
#if defined(ZN_GODOT)
	Vector3 _b_get_motion(Vector3 p_pos, Vector3 p_motion, AABB p_aabb, Node *p_terrain_node);
#elif defined(ZN_GODOT_EXTENSION)
	// TODO GDX: it seems binding a method taking a `Node*` fails to compile. It is supposed to be working.
	Vector3 _b_get_motion(Vector3 p_pos, Vector3 p_motion, AABB p_aabb, Object *p_terrain_node_o);
#endif

	static void _bind_methods();

	// Config
	uint32_t _collision_mask = 0xffffffff; // Everything
	bool _step_climbing_enabled = false;
	real_t _max_step_height = 0.5;

	// States
	bool _has_stepped_up = false;
};

} // namespace zylann::voxel

#endif // VOXEL_BOX_MOVER_H
