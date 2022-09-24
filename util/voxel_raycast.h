#include "../util/math/vector3i.h"
//#include "../util/profiling.h"
#include "errors.h"
#include "math/vector3.h"

namespace zylann {

// Values known when crossing one cell.
// View of the cell where the ray goes from A to B:
//
//    |       /|
//  --o------A-o--
//    |prev /  |
//    |    /   |
//    |   /    |
//  --o--B-----o--
//    |current |
//
struct VoxelRaycastState {
	// Grid position of the cell we are coming from.
	Vector3i hit_prev_position;
	// Distance along the ray where we entered the previous cell
	float prev_distance;
	// Grid position of the cell we just hit
	Vector3i hit_position;
	// Distance along the ray where we enter the hit cell
	float distance;
};

// Runs the DDA algorithms in 3D.
template <typename Vec3f_T, typename Predicate_F> // f(VoxelRaycastState) -> bool
bool voxel_raycast(Vec3f_T ray_origin, Vec3f_T ray_direction, Predicate_F predicate, real_t max_distance,
		Vector3i &out_hit_pos, Vector3i &out_prev_pos, float &out_distance_along_ray,
		float &out_distance_along_ray_prev) {
	//ZN_PROFILE_SCOPE();

	ZN_ASSERT_RETURN_V(!math::has_nan(ray_origin), false);
	ZN_ASSERT_RETURN_V(!math::has_nan(ray_direction), false);
	ZN_ASSERT_RETURN_V(!math::is_nan(max_distance), false);

	const float g_infinite = 9999999;

	// Equation : p + v*t
	// p : ray start position (ray.pos)
	// v : ray orientation vector (ray.dir)
	// t : parametric variable = a distance if v is normalized

	// This raycasting technique is described here :
	// http://www.cse.yorku.ca/~amana/research/grid.pdf

	// See also https://www.youtube.com/watch?v=NbSee-XM7WA

	// Note : the grid is assumed to have 1-unit square cells.

	ZN_ASSERT_RETURN_V(math::is_normalized(ray_direction), false); // Must be normalized

	/* Initialisation */

	// Voxel position
	Vector3i hit_pos = math::floor_to_int(ray_origin);
	Vector3i hit_prev_pos = hit_pos;

	// Voxel step
	const int xi_step = ray_direction.x > 0 ? 1 : ray_direction.x < 0 ? -1 : 0;
	const int yi_step = ray_direction.y > 0 ? 1 : ray_direction.y < 0 ? -1 : 0;
	const int zi_step = ray_direction.z > 0 ? 1 : ray_direction.z < 0 ? -1 : 0;

	// Parametric voxel step
	const real_t tdelta_x = xi_step != 0 ? 1.f / Math::abs(ray_direction.x) : g_infinite;
	const real_t tdelta_y = yi_step != 0 ? 1.f / Math::abs(ray_direction.y) : g_infinite;
	const real_t tdelta_z = zi_step != 0 ? 1.f / Math::abs(ray_direction.z) : g_infinite;

	// Parametric grid-cross
	real_t tcross_x; // At which value of T we will cross a vertical line?
	real_t tcross_y; // At which value of T we will cross a horizontal line?
	real_t tcross_z; // At which value of T we will cross a depth line?

	// X initialization
	if (xi_step != 0) {
		if (xi_step == 1) {
			tcross_x = (Math::ceil(ray_origin.x) - ray_origin.x) * tdelta_x;
		} else {
			tcross_x = (ray_origin.x - Math::floor(ray_origin.x)) * tdelta_x;
		}
	} else {
		tcross_x = g_infinite; // Will never cross on X
	}

	// Y initialization
	if (yi_step != 0) {
		if (yi_step == 1) {
			tcross_y = (Math::ceil(ray_origin.y) - ray_origin.y) * tdelta_y;
		} else {
			tcross_y = (ray_origin.y - Math::floor(ray_origin.y)) * tdelta_y;
		}
	} else {
		tcross_y = g_infinite; // Will never cross on X
	}

	// Z initialization
	if (zi_step != 0) {
		if (zi_step == 1) {
			tcross_z = (Math::ceil(ray_origin.z) - ray_origin.z) * tdelta_z;
		} else {
			tcross_z = (ray_origin.z - Math::floor(ray_origin.z)) * tdelta_z;
		}
	} else {
		tcross_z = g_infinite; // Will never cross on X
	}

	// Workaround for integer positions
	// Adapted from https://github.com/bulletphysics/bullet3/blob/3dbe5426bf7387e532c17df9a1c5e5a4972c298a/src/
	// BulletCollision/CollisionShapes/btHeightfieldTerrainShape.cpp#L418
	if (tcross_x == 0.0) {
		tcross_x += tdelta_x;
		// If going backwards, we should ignore the position we would get by the above flooring,
		// because the ray is not heading in that direction
		if (xi_step == -1) {
			hit_pos.x -= 1;
		}
	}

	if (tcross_y == 0.0) {
		tcross_y += tdelta_y;
		if (yi_step == -1) {
			hit_pos.y -= 1;
		}
	}

	if (tcross_z == 0.0) {
		tcross_z += tdelta_z;
		if (zi_step == -1) {
			hit_pos.z -= 1;
		}
	}

	/* Iteration */

	float t = 0.f;
	float t_prev = 0.f;

	do {
		hit_prev_pos = hit_pos;
		t_prev = t;
		if (tcross_x < tcross_y) {
			if (tcross_x < tcross_z) {
				// X collision
				//hit.prevPos.x = hit.pos.x;
				hit_pos.x += xi_step;
				if (tcross_x > max_distance) {
					return false;
				}
				t = tcross_x;
				tcross_x += tdelta_x;
			} else {
				// Z collision (duplicate code)
				//hit.prevPos.z = hit.pos.z;
				hit_pos.z += zi_step;
				if (tcross_z > max_distance) {
					return false;
				}
				t = tcross_z;
				tcross_z += tdelta_z;
			}
		} else {
			if (tcross_y < tcross_z) {
				// Y collision
				//hit.prevPos.y = hit.pos.y;
				hit_pos.y += yi_step;
				if (tcross_y > max_distance) {
					return false;
				}
				t = tcross_y;
				tcross_y += tdelta_y;
			} else {
				// Z collision (duplicate code)
				//hit.prevPos.z = hit.pos.z;
				hit_pos.z += zi_step;
				if (tcross_z > max_distance) {
					return false;
				}
				t = tcross_z;
				tcross_z += tdelta_z;
			}
		}

	} while (!predicate({ hit_prev_pos, t_prev, hit_pos, t }));

	out_hit_pos = hit_pos;
	out_prev_pos = hit_prev_pos;
	out_distance_along_ray = t;
	out_distance_along_ray_prev = t_prev;

	return true;
}

} // namespace zylann
