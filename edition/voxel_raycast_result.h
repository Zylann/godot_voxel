#ifndef VOXEL_RAYCAST_RESULT_H
#define VOXEL_RAYCAST_RESULT_H

#include "../util/math/vector3i.h"
#include <core/object/ref_counted.h>

// This class exists only to make the script API nicer.
class VoxelRaycastResult : public RefCounted {
	GDCLASS(VoxelRaycastResult, RefCounted)
public:
	VOX_Vector3i position;
	VOX_Vector3i previous_position;
	float distance_along_ray;

private:
	Vector3 _b_get_position() const;
	Vector3 _b_get_previous_position() const;
	float _b_get_distance() const;

	static void _bind_methods();
};

#endif // VOXEL_RAYCAST_RESULT_H
