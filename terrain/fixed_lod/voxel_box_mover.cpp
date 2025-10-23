#include "voxel_box_mover.h"
#include "../../meshers/blocky/voxel_mesher_blocky.h"
#include "../../meshers/cubes/voxel_mesher_cubes.h"
#include "../../storage/voxel_buffer.h"
#include "../../storage/voxel_data.h"
#include "../../util/containers/std_vector.h"
#include "../../util/profiling.h"
// #include "../../util/string/format.h"
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

	// TODO Candidate for temp allocator
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

inline Vector2 get_xz(Vector3 v) {
	return Vector2(v.x, v.z);
}

// Finds the first obstacle to a rectangle moving down through axis-aligned boxes
real_t rect_cast_down(Span<const AABB> boxes, const Rect2 rect, const real_t from_y, const real_t min_y) {
	real_t hit_y = min_y;
	for (const AABB box : boxes) {
		if (box.position.y > from_y) {
			// Box is above starting position
			continue;
		}
		const real_t box_top = box.position.y + box.size.y;
		if (box_top > from_y) {
			// Ignore overlap
			continue;
		}
		if (box_top < min_y) {
			// Too far
			continue;
		}
		if (!rect.intersects(Rect2(get_xz(box.position), get_xz(box.size)))) {
			continue;
		}
		hit_y = box_top;
	}
	return hit_y;
}

// Finds the first obstacle to a rectangle moving up through axis-aligned boxes
real_t rect_cast_up(Span<const AABB> boxes, const Rect2 rect, const real_t from_y, const real_t max_y) {
	real_t hit_y = max_y;
	for (const AABB box : boxes) {
		if (box.position.y + box.size.y < from_y) {
			// Box is below starting position
			continue;
		}
		if (box.position.y < from_y) {
			// Ignore overlap
			continue;
		}
		if (box.position.y > max_y) {
			// Too far
			continue;
		}
		if (!rect.intersects(Rect2(get_xz(box.position), get_xz(box.size)))) {
			continue;
		}
		hit_y = math::min(hit_y, box.position.y);
	}
	return hit_y;
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

		const blocky::BakedLibrary &baked_data = library_ref->get_baked_data();

		// TODO Optimization: read the whole box of voxels at once, querying individually is slower
		for (i.z = min_z; i.z < max_z; ++i.z) {
			for (i.y = min_y; i.y < max_y; ++i.y) {
				for (i.x = min_x; i.x < max_x; ++i.x) {
					const int type_id = voxels.get_voxel(i, channel, defval).i;

					if (baked_data.has_model(type_id)) {
						const blocky::BakedModel &model = baked_data.models[type_id];

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
	const Vector3 input_motion = to_local.basis.xform(p_motion);
	const AABB aabb = Transform3D(to_local.basis, Vector3()).xform(p_aabb);

	const AABB box(aabb.position + pos, aabb.size);

	AABB expanded_box = expand_with_vector(box, input_motion);
	if (_step_climbing_enabled) {
		// We'll have to gather a bit higher for ceilings in case we have to climb up steps
		expanded_box.size.y += _max_step_height;
	}

	static thread_local StdVector<AABB> s_colliding_boxes;
	StdVector<AABB> &potential_boxes = s_colliding_boxes;
	potential_boxes.clear();

	// Collect potential collisions with the terrain (broad phase)
	// TODO If motion is really big, we may want something more optimal or reject it
	collect_boxes(p_terrain, expanded_box, _collision_mask, potential_boxes);

	const Vector3 slided_motion1 = zylann::voxel::get_motion(box, input_motion, to_span(potential_boxes));
	Vector3 final_motion = slided_motion1;

	_has_stepped_up = false;

	if (_step_climbing_enabled &&
		// Movement is horizontal?
		Math::abs(slided_motion1.y) < 0.001 && get_xz(input_motion).length_squared() > 0.0001 &&
		// Horizontal direction of input motion isn't the same as resulting slided motion?
		get_xz(input_motion).distance_squared_to(get_xz(slided_motion1)) > 0.1 * 0.1) {
		//
		AABB mobox = box;

		// Raise the box as high as max step height and ceilings allow
		{
			const real_t me_top = box.position.y + box.size.y;
			const real_t hit_h = rect_cast_up(
					to_span(potential_boxes),
					Rect2(get_xz(box.position), get_xz(box.size)),
					me_top,
					me_top + _max_step_height
			);
			const real_t step_h = hit_h - me_top;
			mobox.position.y += step_h;
		}

		// Account for gravity that got cancelled by the first attempt,
		// which would bias our step height. We're supposed to be on ground anyways
		const Vector3 momotion(input_motion.x, slided_motion1.y, input_motion.z);

		// Do a second attempt at moving
		Vector3 slided_motion2 = zylann::voxel::get_motion(mobox, momotion, to_span(potential_boxes));

		{
			// Move the box back down as far as we raised it or until we touch ground
			const real_t step_h = mobox.position.y - box.position.y;
			const Vector3 slided_pos = mobox.position + slided_motion2;
			const real_t epsilon = 0.0001;
			const real_t hit_y = rect_cast_down(
					to_span(potential_boxes),
					Rect2(get_xz(slided_pos), get_xz(box.size)),
					slided_pos.y + epsilon,
					slided_pos.y - step_h
			);
			const real_t effective_step_h = math::max<real_t>(hit_y + epsilon - box.position.y, 0);
			slided_motion2.y += effective_step_h;
		}

		if (slided_motion2.length_squared() > slided_motion1.length_squared()) {
			// print_line(
			// 		format("Preferring step climb {} over {} (going from {} to {})",
			// 			   slided_motion2,
			// 			   slided_motion1,
			// 			   p_pos,
			// 			   p_pos + slided_motion2)
			// );
			final_motion = slided_motion2;
			_has_stepped_up = true;
		}
	}

	// Switch back to world
	const Vector3 world_slided_motion = to_world.basis.xform(final_motion);

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
