#include "voxel_box_mover.h"
#include "../../meshers/blocky/voxel_mesher_blocky.h"
#include "../../meshers/cubes/voxel_mesher_cubes.h"
#include "../../storage/voxel_buffer.h"
#include "../../storage/voxel_data.h"
#include "../../util/containers/std_vector.h"
#include "../../util/profiling.h"
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

void collect_boxes(VoxelTerrain &p_terrain, AABB query_box, uint32_t collision_nask, StdVector<AABB> &potential_boxes) {
	ZN_PROFILE_SCOPE();
	const VoxelData &voxels = p_terrain.get_storage();

	const int min_x = int(Math::floor(query_box.position.x));
	const int min_y = int(Math::floor(query_box.position.y));
	const int min_z = int(Math::floor(query_box.position.z));

	const Vector3 query_box_end = query_box.position + query_box.size;
	const int max_x = int(Math::ceil(query_box_end.x));
	const int max_y = int(Math::ceil(query_box_end.y));
	const int max_z = int(Math::ceil(query_box_end.z));

	Vector3i i(min_x, min_y, min_z);

	Ref<VoxelMesherBlocky> mesher_blocky;
	Ref<VoxelMesherCubes> mesher_cubes;

	if (zylann::godot::try_get_as(p_terrain.get_mesher(), mesher_blocky)) {
		Ref<VoxelBlockyLibraryBase> library_ref = mesher_blocky->get_library();
		ERR_FAIL_COND_MSG(library_ref.is_null(), "VoxelMesherBlocky has no library assigned");
		const int channel = VoxelBuffer::CHANNEL_TYPE;
		VoxelSingleValue defval;
		defval.i = 0;

		const VoxelBlockyLibraryBase::BakedData &baked_data = library_ref->get_baked_data();

		// TODO Optimization: read the whole box of voxels at once, querying individually is slower
		for (i.z = min_z; i.z < max_z; ++i.z) {
			for (i.y = min_y; i.y < max_y; ++i.y) {
				for (i.x = min_x; i.x < max_x; ++i.x) {
					const int type_id = voxels.get_voxel(i, channel, defval).i;

					if (baked_data.has_model(type_id)) {
						const VoxelBlockyModel::BakedData &model = baked_data.models[type_id];

						if ((model.box_collision_mask & collision_nask) == 0) {
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

	} else if (zylann::godot::try_get_as(p_terrain.get_mesher(), mesher_cubes)) {
		const int channel = VoxelBuffer::CHANNEL_COLOR;
		VoxelSingleValue defval;
		defval.i = 0;

		// TODO Optimization: read the whole box of voxels at once, querying individually is slower
		for (i.z = min_z; i.z < max_z; ++i.z) {
			for (i.y = min_y; i.y < max_y; ++i.y) {
				for (i.x = min_x; i.x < max_x; ++i.x) {
					const int color_data = voxels.get_voxel(i, channel, defval).i;
					if (color_data != 0) {
						potential_boxes.push_back(AABB(i, Vector3(1, 1, 1)));
					}
				}
			}
		}
	}
}

} // namespace

Vector3 VoxelBoxMover::get_motion(Vector3 p_pos, Vector3 p_motion, AABB p_aabb, VoxelTerrain &p_terrain) {
	ZN_PROFILE_SCOPE();
	// The mesher is required to know how collisions should be processed
	ERR_FAIL_COND_V(p_terrain.get_mesher().is_null(), Vector3());

	// Transform to local in case the volume is transformed
	const Transform3D to_world = p_terrain.get_global_transform();
	const Transform3D to_local = to_world.affine_inverse();
	const Vector3 pos = to_local.xform(p_pos);
	const Vector3 motion = to_local.basis.xform(p_motion);
	const AABB aabb = Transform3D(to_local.basis, Vector3()).xform(p_aabb);

	const AABB box(aabb.position + pos, aabb.size);
	const AABB expanded_box = expand_with_vector(box, motion);

	static thread_local StdVector<AABB> s_colliding_boxes;
	StdVector<AABB> &potential_boxes = s_colliding_boxes;
	potential_boxes.clear();

	// Collect potential collisions with the terrain (broad phase)
	// TODO If motion is really big, we may want something more optimal or reject it
	collect_boxes(p_terrain, expanded_box, _collision_mask, potential_boxes);

	// Calculate collisions (narrow phase)
	Vector3 slided_motion = zylann::voxel::get_motion(box, motion, to_span(potential_boxes));

	// Minecraft-style stair climbing:
	// If we were moving, changed horizontal direction due to collision, and resulting motion is about horizontal
	_has_stepped_up = false;
	if (_step_climbing_enabled &&
			// Movement is horizontal?
			Math::abs(slided_motion.y) < 0.001 && Vector2(motion.x, motion.z).length_squared() > 0.0001 &&
			// Motor movement isn't the same as resulting slided motion?
			Vector2(motion.x, motion.z).normalized().dot(Vector2(slided_motion.x, slided_motion.z).normalized()) <
					0.99) {
		// We hit an obstacle
		real_t hit_y;
		// Find out the height of the step
		if (boxcast_down(to_span(potential_boxes), get_xz(expanded_box.position), get_xz(expanded_box.size), hit_y)) {
			// If the step is up and not too high
			if (hit_y > box.position.y && (hit_y - box.position.y) <= _max_step_height) {
				// Check if we would fit if we move the box above the step.
				// Raise it slightly higher to avoid precision issues. Even if the final motion would move the box
				// exactly on top of the stair, gameplay code could do some additional calculations with that motion
				// (converting it to velocity?) which may induce precision errors causing the box to fall through.
				const real_t epsilon = 0.0001f;
				const AABB hyp_box(
						Vector3(box.position.x + motion.x, hit_y + epsilon, box.position.z + motion.z), box.size);

				potential_boxes.clear();
				collect_boxes(p_terrain, hyp_box, _collision_mask, potential_boxes);

				// If the box fits on top of the step
				if (!intersects(to_span(potential_boxes), hyp_box)) {
					// Change motion so that it brings the box on top of the step
					slided_motion = hyp_box.position - box.position;
					_has_stepped_up = true;
				}
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

#if defined(ZN_GODOT)
Vector3 VoxelBoxMover::_b_get_motion(Vector3 pos, Vector3 motion, AABB aabb, Node *terrain_node) {
#elif defined(ZN_GODOT_EXTENSION)
Vector3 VoxelBoxMover::_b_get_motion(Vector3 pos, Vector3 motion, AABB aabb, Object *terrain_node_o) {
	Node *terrain_node = Object::cast_to<Node>(terrain_node_o);
#endif
	ERR_FAIL_COND_V(terrain_node == nullptr, Vector3());
	VoxelTerrain *terrain = Object::cast_to<VoxelTerrain>(terrain_node);
	ERR_FAIL_COND_V(terrain == nullptr, Vector3());
	return get_motion(pos, motion, aabb, *terrain);
}

void VoxelBoxMover::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_motion", "pos", "motion", "aabb", "terrain"), &VoxelBoxMover::_b_get_motion);

	ClassDB::bind_method(D_METHOD("set_collision_mask", "mask"), &VoxelBoxMover::set_collision_mask);
	ClassDB::bind_method(D_METHOD("get_collision_mask"), &VoxelBoxMover::get_collision_mask);

	ClassDB::bind_method(D_METHOD("set_step_climbing_enabled", "enabled"), &VoxelBoxMover::set_step_climbing_enabled);
	ClassDB::bind_method(D_METHOD("is_step_climbing_enabled"), &VoxelBoxMover::is_step_climbing_enabled);

	ClassDB::bind_method(D_METHOD("set_max_step_height", "height"), &VoxelBoxMover::set_max_step_height);
	ClassDB::bind_method(D_METHOD("get_max_step_height"), &VoxelBoxMover::get_max_step_height);

	ClassDB::bind_method(D_METHOD("has_stepped_up"), &VoxelBoxMover::has_stepped_up);
}

} // namespace zylann::voxel
