#include "../math/vector3i.h"
#include <core/math/vector3.h>

// TODO that could be a template function
// pos: voxel position
// context: arguments to carry (as a lamdbda capture)
typedef bool (*VoxelPredicate)(Vector3i pos, void *context);

bool voxel_raycast(
		Vector3 ray_origin,
		Vector3 ray_direction,
		VoxelPredicate predicate,
		void *predicate_context, // Handle that one with care
		real_t max_distance,
		Vector3i &out_hit_pos,
		Vector3i &out_prev_pos);
