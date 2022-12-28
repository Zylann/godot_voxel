#ifndef VOXEL_RAYCAST_RESULT_H
#define VOXEL_RAYCAST_RESULT_H

#include "../util/godot/classes/ref_counted.h"
#include "../util/math/vector3i.h"

namespace zylann::voxel {

// This class exists only to make the script API nicer.
class VoxelRaycastResult : public RefCounted {
	GDCLASS(VoxelRaycastResult, RefCounted)
public:
	Vector3i position;
	Vector3i previous_position;
	float distance_along_ray;

private:
	Vector3i _b_get_position() const;
	Vector3i _b_get_previous_position() const;
	float _b_get_distance() const;

	static void _bind_methods();
};

} // namespace zylann::voxel

#endif // VOXEL_RAYCAST_RESULT_H
