#include "voxel_raycast.h"
#include <core/math/math_funcs.h>

bool voxel_raycast(
		Vector3 ray_origin,
		Vector3 ray_direction,
		VoxelPredicate predicate,
		void *predicate_context,
		real_t max_distance,
		Vector3i &out_hit_pos,
		Vector3i &out_prev_pos) {

	const float g_infinite = 9999999;

	// Equation : p + v*t
	// p : ray start position (ray.pos)
	// v : ray orientation vector (ray.dir)
	// t : parametric variable = a distance if v is normalized

	// This raycasting technique is described here :
	// http://www.cse.yorku.ca/~amana/research/grid.pdf

	// Note : the grid is assumed to have 1-unit square cells.

	ERR_FAIL_COND_V(predicate == 0, false);
	ERR_FAIL_COND_V(ray_direction.is_normalized() == false, false); // Must be normalized

	/* Initialisation */

	// Voxel position
	Vector3i hit_pos(
			Math::floor(ray_origin.x),
			Math::floor(ray_origin.y),
			Math::floor(ray_origin.z));
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
		if (xi_step == 1)
			tcross_x = (Math::ceil(ray_origin.x) - ray_origin.x) * tdelta_x;
		else
			tcross_x = (ray_origin.x - Math::floor(ray_origin.x)) * tdelta_x;
	} else
		tcross_x = g_infinite; // Will never cross on X

	// Y initialization
	if (yi_step != 0) {
		if (yi_step == 1)
			tcross_y = (Math::ceil(ray_origin.y) - ray_origin.y) * tdelta_y;
		else
			tcross_y = (ray_origin.y - Math::floor(ray_origin.y)) * tdelta_y;
	} else
		tcross_y = g_infinite; // Will never cross on X

	// Z initialization
	if (zi_step != 0) {
		if (zi_step == 1)
			tcross_z = (Math::ceil(ray_origin.z) - ray_origin.z) * tdelta_z;
		else
			tcross_z = (ray_origin.z - Math::floor(ray_origin.z)) * tdelta_z;
	} else
		tcross_z = g_infinite; // Will never cross on X

	/* Iteration */

	do {
		hit_prev_pos = hit_pos;
		if (tcross_x < tcross_y) {
			if (tcross_x < tcross_z) {
				// X collision
				//hit.prevPos.x = hit.pos.x;
				hit_pos.x += xi_step;
				if (tcross_x > max_distance)
					return false;
				tcross_x += tdelta_x;
			} else {
				// Z collision (duplicate code)
				//hit.prevPos.z = hit.pos.z;
				hit_pos.z += zi_step;
				if (tcross_z > max_distance)
					return false;
				tcross_z += tdelta_z;
			}
		} else {
			if (tcross_y < tcross_z) {
				// Y collision
				//hit.prevPos.y = hit.pos.y;
				hit_pos.y += yi_step;
				if (tcross_y > max_distance)
					return false;
				tcross_y += tdelta_y;
			} else {
				// Z collision (duplicate code)
				//hit.prevPos.z = hit.pos.z;
				hit_pos.z += zi_step;
				if (tcross_z > max_distance)
					return false;
				tcross_z += tdelta_z;
			}
		}

	} while (!predicate(hit_pos, predicate_context));

	out_hit_pos = hit_pos;
	out_prev_pos = hit_prev_pos;

	return true;
}
