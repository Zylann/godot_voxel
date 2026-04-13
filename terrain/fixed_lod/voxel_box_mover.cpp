#include "voxel_box_mover.h"
#include "../../meshers/blocky/voxel_mesher_blocky.h"
#include "../../meshers/cubes/voxel_mesher_cubes.h"
#include "../../storage/voxel_buffer.h"
#include "../../storage/voxel_data.h"
#include "../../util/containers/std_vector.h"
#include "../../util/profiling.h"
#include "../variable_lod/voxel_lod_terrain.h"
#include "voxel_terrain.h"

namespace zylann::voxel {

namespace {

AABB expand_with_vector(AABB box, Vector3 v) {
	if (v.x > 0) {
		box.size.x += v.x;
	} else if (v.x < 0) {
		box.position.x += v.x;
		box.size.x -= v.x;
	}

	if (v.y > 0) {
		box.size.y += v.y;
	} else if (v.y < 0) {
		box.position.y += v.y;
		box.size.y -= v.y;
	}

	if (v.z > 0) {
		box.size.z += v.z;
	} else if (v.z < 0) {
		box.position.z += v.z;
		box.size.z -= v.z;
	}

	return box;
}

real_t calculate_i_offset(const AABB &box, AABB other, real_t motion, int i, int j, int k) {
	const real_t EPSILON = 0.001;

	const Vector3 other_end = other.position + other.size;
	const Vector3 box_end = box.position + box.size;

	if (other_end[k] <= box.position[k] || other.position[k] >= box_end[k]) {
		return motion;
	}

	if (other_end[j] <= box.position[j] || other.position[j] >= box_end[j]) {
		return motion;
	}

	if (motion > 0.0 && other_end[i] <= box.position[i]) {
		const real_t off = box.position[i] - other_end[i] - EPSILON;
		if (off < motion) {
			motion = off;
		}
	}

	if (motion < 0.0 && other.position[i] >= box_end[i]) {
		const real_t off = box_end[i] - other.position[i] + EPSILON;
		if (off > motion) {
			motion = off;
		}
	}

	return motion;
}

// Gets the transformed vector for moving a box and slide.
// This algorithm is free from tunnelling for axis-aligned movement,
// except in some high-speed diagonal cases or huge size differences:
// For example, if a box is fast enough to have a diagonal motion jumping from A to B,
// it will pass through C if that other box is the only other one:
//
//  o---o
//  | A |
//  o---o
//          o---o
//          | C |
//          o---o
//                  o---o
//                  | B |
//                  o---o
//
// TODO one way to fix this would be to try a "hot side" projection instead
//
Vector3 get_motion(AABB box, Vector3 motion, Span<const AABB> environment_boxes) {
	// The bounding box is expanded to include it's estimated version at next update.
	// This also makes the algorithm tunnelling-free
	const AABB expanded_box = expand_with_vector(box, motion);

	StdVector<AABB> colliding_boxes;
	for (size_t i = 0; i < environment_boxes.size(); ++i) {
		const AABB &other = environment_boxes[i];
		if (expanded_box.intersects(other)) {
			colliding_boxes.push_back(other);
		}
	}

	if (colliding_boxes.size() == 0) {
		return motion;
	}

	// print("Colliding: ", colliding_boxes.size())

	Vector3 new_motion = motion;

	for (unsigned int i = 0; i < colliding_boxes.size(); ++i) {
		new_motion.y = calculate_i_offset(colliding_boxes[i], box, new_motion.y, 1, 0, 2);
	}
	box.position.y += new_motion.y;

	for (unsigned int i = 0; i < colliding_boxes.size(); ++i) {
		new_motion.x = calculate_i_offset(colliding_boxes[i], box, new_motion.x, 0, 1, 2);
	}
	box.position.x += new_motion.x;

	for (unsigned int i = 0; i < colliding_boxes.size(); ++i) {
		new_motion.z = calculate_i_offset(colliding_boxes[i], box, new_motion.z, 2, 1, 0);
	}
	box.position.z += new_motion.z;

	return new_motion;
}

// bool raycast_down(Span<const AABB> aabbs, Vector3 ray_origin, real_t &out_hit_y) {
// 	if (aabbs.size() == 0) {
// 		return false;
// 	}
// 	bool hit = false;
// 	real_t max_y = -9999999;
// 	for (unsigned int i = 0; i < aabbs.size(); ++i) {
// 		const AABB &aabb = aabbs[i];
// 		if ( //
// 				ray_origin.x >= aabb.position.x && //
// 				ray_origin.z >= aabb.position.z && //
// 				ray_origin.x < aabb.position.x + aabb.size.x && //
// 				ray_origin.z < aabb.position.z + aabb.size.z) {
// 			//
// 			const real_t box_top = aabb.position.y + aabb.size.y;
// 			if (hit) {
// 				max_y = math::max(box_top, max_y);
// 			} else {
// 				max_y = box_top;
// 			}
// 			hit = true;
// 		}
// 	}
// 	out_hit_y = max_y;
// 	return hit;
// }

inline Vector2 get_xz(Vector3 v) {
	return Vector2(v.x, v.z);
}

bool boxcast_down(Span<const AABB> aabbs, Vector2 box_pos, Vector2 box_size, real_t &out_hit_y) {
	if (aabbs.size() == 0) {
		return false;
	}
	bool hit = false;
	real_t max_y = -9999999;
	for (unsigned int i = 0; i < aabbs.size(); ++i) {
		const AABB &aabb = aabbs[i];
		if (Rect2(box_pos, box_size).intersects(Rect2(get_xz(aabb.position), get_xz(aabb.size)))) {
			const real_t box_top = aabb.position.y + aabb.size.y;
			if (hit) {
				max_y = math::max(box_top, max_y);
			} else {
				max_y = box_top;
			}
			hit = true;
		}
	}
	out_hit_y = max_y;
	return hit;
}

bool intersects(Span<const AABB> aabbs, const AABB &box) {
	for (unsigned int i = 0; i < aabbs.size(); ++i) {
		if (aabbs[i].intersects(box)) {
			return true;
		}
	}
	return false;
}

void collect_boxes_blocky(
		const VoxelData &voxels,
		const VoxelMesherBlocky &mesher,
		const Vector3i minp,
		const Vector3i maxp,
		const uint32_t collision_mask,
		StdVector<AABB> &potential_boxes
) {
	Ref<VoxelBlockyLibraryBase> library_ref = mesher.get_library();
	ERR_FAIL_COND_MSG(library_ref.is_null(), "VoxelMesherBlocky has no library assigned");
	const int channel = VoxelBuffer::CHANNEL_TYPE;
	VoxelSingleValue defval;
	defval.i = 0;

	const blocky::BakedLibrary &baked_data = library_ref->get_baked_data();

	Vector3i i = minp;

	// TODO Optimization: read the whole box of voxels at once, querying individually is slower
	for (i.z = minp.z; i.z < maxp.z; ++i.z) {
		for (i.y = minp.y; i.y < maxp.y; ++i.y) {
			for (i.x = minp.x; i.x < maxp.x; ++i.x) {
				const int type_id = voxels.get_voxel(i, channel, defval).i;

				if (baked_data.has_model(type_id)) {
					const blocky::BakedModel &model = baked_data.models[type_id];

					if ((model.box_collision_mask & collision_mask) == 0) {
						continue;
					}

					for (const AABB &aabb : model.box_collision_aabbs) {
						AABB world_box = aabb;
						world_box.position += i;
						potential_boxes.push_back(world_box);
					}
				}
			}
		}
	}
}

void collect_boxes_cubes(
		const VoxelData &voxels,
		const Vector3i minp,
		const Vector3i maxp,
		StdVector<AABB> &potential_boxes
) {
	const int channel = VoxelBuffer::CHANNEL_COLOR;
	VoxelSingleValue defval;
	defval.i = 0;

	Vector3i i = minp;

	// TODO Optimization: read the whole box of voxels at once, querying individually is slower
	for (i.z = minp.z; i.z < maxp.z; ++i.z) {
		for (i.y = minp.y; i.y < maxp.y; ++i.y) {
			for (i.x = minp.x; i.x < maxp.x; ++i.x) {
				const int color_data = voxels.get_voxel(i, channel, defval).i;
				if (color_data != 0) {
					potential_boxes.push_back(AABB(i, Vector3(1, 1, 1)));
				}
			}
		}
	}
}

void collect_boxes(
		const VoxelData &voxels,
		const VoxelMesher &mesher,
		const AABB query_box,
		const uint32_t collision_nask,
		StdVector<AABB> &potential_boxes
) {
	ZN_PROFILE_SCOPE();

	const Vector3i minp = math::floor_to_int(query_box.position);
	const Vector3i maxp = math::ceil_to_int(query_box.position + query_box.size);

	const VoxelMesherBlocky *mesher_blocky = Object::cast_to<VoxelMesherBlocky>(&mesher);
	if (mesher_blocky != nullptr) {
		collect_boxes_blocky(voxels, *mesher_blocky, minp, maxp, collision_nask, potential_boxes);
		return;
	}

	const VoxelMesherCubes *mesher_cubes = Object::cast_to<VoxelMesherCubes>(&mesher);
	if (mesher_cubes != nullptr) {
		collect_boxes_cubes(voxels, minp, maxp, potential_boxes);
		return;
	}
}

} // namespace

Vector3 VoxelBoxMover::get_motion(
		const Vector3 pos_world,
		const Vector3 motion_world,
		const AABB aabb_world,
		const VoxelData &terrain_data,
		const Transform3D &terrain_transform,
		const VoxelMesher &mesher
) {
	ZN_PROFILE_SCOPE();

	// Transform to local in case the volume is transformed
	const Transform3D to_world = terrain_transform;
	const Transform3D to_local = to_world.affine_inverse();
	const Vector3 pos = to_local.xform(pos_world);
	const Vector3 motion = to_local.basis.xform(motion_world);
	const AABB aabb = Transform3D(to_local.basis, Vector3()).xform(aabb_world);

	const AABB box(aabb.position + pos, aabb.size);
	const AABB expanded_box = expand_with_vector(box, motion);

	// TODO Candidate for temp allocator
	static thread_local StdVector<AABB> s_colliding_boxes;
	StdVector<AABB> &potential_boxes = s_colliding_boxes;
	potential_boxes.clear();

	// Collect potential collisions with the terrain (broad phase)
	// TODO If motion is really big, we may want something more optimal or reject it
	collect_boxes(terrain_data, mesher, expanded_box, _collision_mask, potential_boxes);

	// Calculate collisions (narrow phase)
	Vector3 slided_motion = zylann::voxel::get_motion(box, motion, to_span(potential_boxes));

	// Minecraft-style stair climbing:
	// If we were moving, changed horizontal direction due to collision, and resulting motion is about horizontal
	_has_stepped_up = false;
	const real_t epsilon = 0.0001f;
	if (_step_climbing_enabled && Math::abs(motion.y) <= 0.001 && Vector2(motion.x, motion.z).length_squared() > epsilon) {
		Vector2 intended_h_motion(motion.x, motion.z);
		Vector2 actual_h_motion(slided_motion.x, slided_motion.z);

		// Only step if we're blocked horizontally, i.e. the actual motion is less than the intended
		if (actual_h_motion.length_squared() < intended_h_motion.length_squared() - epsilon) {
			// Basically we're now going to manually push the player up to the max step height, check for the possible horizontal motion,
			// push the player back down to the "real" step height of the terrain, and then check if we've gotten farther than before.
			AABB step_box = box; // Starting box
			
			// Lift box manually to max step height, use get_motion() incase we're blocked by a ceiling
			Vector3 lift_motion = zylann::voxel::get_motion(step_box, Vector3(0, _max_step_height, 0), to_span(potential_boxes));
			step_box.position.y += lift_motion.y;

			// Try to get the horizontal motion in this "lifted" mode
			Vector3 h_motion_elev = zylann::voxel::get_motion(step_box, Vector3(motion.x, 0, motion.z), to_span(potential_boxes));
			step_box.position.x += h_motion_elev.x;
			step_box.position.z += h_motion_elev.z;

			// Push the player back down to the "real" terrain/step floor
			Vector3 drop_motion = zylann::voxel::get_motion(step_box, Vector3(0, -lift_motion.y, 0), to_span(potential_boxes));
			step_box.position.y += drop_motion.y;

			// Check if the distance using this "push-up" method is greater than the initial motion
			Vector2 step_h_diff(step_box.position.x - box.position.x, step_box.position.z - box.position.z);
			
			if (step_h_diff.length_squared() > actual_h_motion.length_squared() + epsilon) {
				slided_motion = step_box.position - box.position;
				_has_stepped_up = true;
			}
		}
	}

	// Switch back to world
	const Vector3 world_slided_motion = to_world.basis.xform(slided_motion);
	return world_slided_motion;
}

void VoxelBoxMover::set_collision_mask(uint32_t mask) {
	_collision_mask = mask;
}

void VoxelBoxMover::set_step_climbing_enabled(bool enable) {
	_step_climbing_enabled = enable;
}

bool VoxelBoxMover::is_step_climbing_enabled() const {
	return _step_climbing_enabled;
}

bool VoxelBoxMover::has_stepped_up() const {
	return _has_stepped_up;
}

void VoxelBoxMover::set_max_step_height(float height) {
	_max_step_height = height;
}

float VoxelBoxMover::get_max_step_height() const {
	return _max_step_height;
}

bool VoxelBoxMover::intersects(
		const AABB aabb_world,
		const VoxelData &terrain_data,
		const Transform3D &terrain_transform,
		const VoxelMesher &mesher
) const {
	// Transform to local in case the volume is transformed
	const Transform3D to_world = terrain_transform;
	const Transform3D to_local = to_world.affine_inverse();
	const AABB aabb = to_local.xform(aabb_world);

	// TODO Candidate for temp allocator
	static thread_local StdVector<AABB> s_colliding_boxes;
	StdVector<AABB> &potential_boxes = s_colliding_boxes;
	potential_boxes.clear();

	// Collect potential collisions with the terrain (broad phase)
	collect_boxes(terrain_data, mesher, aabb, _collision_mask, potential_boxes);

	return zylann::voxel::intersects(to_span(potential_boxes), aabb);
}

#if defined(ZN_GODOT)
Vector3 VoxelBoxMover::_b_get_motion(Vector3 pos, Vector3 motion, AABB aabb, Node *terrain_node) {
#elif defined(ZN_GODOT_EXTENSION)
Vector3 VoxelBoxMover::_b_get_motion(Vector3 pos, Vector3 motion, AABB aabb, Object *terrain_node) {
#endif
	ERR_FAIL_COND_V(terrain_node == nullptr, Vector3());
	VoxelNode *terrain = Object::cast_to<VoxelNode>(terrain_node);
	ERR_FAIL_COND_V(terrain == nullptr, Vector3());

	// The mesher is required to know how collisions should be processed
	Ref<VoxelMesher> mesher = terrain->get_mesher();
	ERR_FAIL_COND_V(mesher.is_null(), Vector3());

	return get_motion(pos, motion, aabb, terrain->get_storage(), terrain->get_global_transform(), **mesher);
}

bool VoxelBoxMover::_b_intersects(AABB p_aabb, Object *p_terrain_node) const {
	ERR_FAIL_COND_V(p_terrain_node == nullptr, false);
	VoxelNode *terrain = Object::cast_to<VoxelNode>(p_terrain_node);
	ERR_FAIL_COND_V(terrain == nullptr, false);

	// The mesher is required to know how collisions should be processed
	Ref<VoxelMesher> mesher = terrain->get_mesher();
	ERR_FAIL_COND_V(mesher.is_null(), false);

	return intersects(p_aabb, terrain->get_storage(), terrain->get_global_transform(), **mesher);
}

void VoxelBoxMover::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_motion", "pos", "motion", "aabb", "terrain"), &VoxelBoxMover::_b_get_motion);
	ClassDB::bind_method(D_METHOD("intersects", "aabb", "terrain"), &VoxelBoxMover::_b_intersects);

	ClassDB::bind_method(D_METHOD("set_collision_mask", "mask"), &VoxelBoxMover::set_collision_mask);
	ClassDB::bind_method(D_METHOD("get_collision_mask"), &VoxelBoxMover::get_collision_mask);

	ClassDB::bind_method(D_METHOD("set_step_climbing_enabled", "enabled"), &VoxelBoxMover::set_step_climbing_enabled);
	ClassDB::bind_method(D_METHOD("is_step_climbing_enabled"), &VoxelBoxMover::is_step_climbing_enabled);

	ClassDB::bind_method(D_METHOD("set_max_step_height", "height"), &VoxelBoxMover::set_max_step_height);
	ClassDB::bind_method(D_METHOD("get_max_step_height"), &VoxelBoxMover::get_max_step_height);

	ClassDB::bind_method(D_METHOD("has_stepped_up"), &VoxelBoxMover::has_stepped_up);
}

} // namespace zylann::voxel
