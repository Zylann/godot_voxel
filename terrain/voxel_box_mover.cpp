#include "voxel_box_mover.h"
#include "../meshers/blocky/voxel_mesher_blocky.h"
#include "../meshers/cubes/voxel_mesher_cubes.h"
#include "../util/godot/funcs.h"

static AABB expand_with_vector(AABB box, Vector3 v) {
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

static float calculate_i_offset(AABB box, AABB other, float motion, int i, int j, int k) {
	const float EPSILON = 0.001;

	Vector3 other_end = other.position + other.size;
	Vector3 box_end = box.position + box.size;

	if (other_end[k] <= box.position[k] || other.position[k] >= box_end[k]) {
		return motion;
	}

	if (other_end[j] <= box.position[j] || other.position[j] >= box_end[j]) {
		return motion;
	}

	if (motion > 0.0 && other_end[i] <= box.position[i]) {
		float off = box.position[i] - other_end[i] - EPSILON;
		if (off < motion) {
			motion = off;
		}
	}

	if (motion < 0.0 && other.position[i] >= box_end[i]) {
		float off = box_end[i] - other.position[i] + EPSILON;
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
static Vector3 get_motion(AABB box, Vector3 motion, const std::vector<AABB> &environment_boxes) {
	// The bounding box is expanded to include it's estimated version at next update.
	// This also makes the algorithm tunnelling-free
	AABB expanded_box = expand_with_vector(box, motion);

	Vector<AABB> colliding_boxes;
	for (size_t i = 0; i < environment_boxes.size(); ++i) {
		const AABB &other = environment_boxes[i];
		if (expanded_box.intersects(other)) {
			colliding_boxes.push_back(other);
		}
	}

	if (colliding_boxes.size() == 0) {
		return motion;
	}

	//print("Colliding: ", colliding_boxes.size())

	Vector3 new_motion = motion;

	for (int i = 0; i < colliding_boxes.size(); ++i) {
		new_motion.y = calculate_i_offset(colliding_boxes[i], box, new_motion.y, 1, 0, 2);
	}
	box.position.y += new_motion.y;

	for (int i = 0; i < colliding_boxes.size(); ++i) {
		new_motion.x = calculate_i_offset(colliding_boxes[i], box, new_motion.x, 0, 1, 2);
	}
	box.position.x += new_motion.x;

	for (int i = 0; i < colliding_boxes.size(); ++i) {
		new_motion.z = calculate_i_offset(colliding_boxes[i], box, new_motion.z, 2, 1, 0);
	}
	box.position.z += new_motion.z;

	return new_motion;
}

Vector3 VoxelBoxMover::get_motion(Vector3 p_pos, Vector3 p_motion, AABB p_aabb, VoxelTerrain *p_terrain) {
	ERR_FAIL_COND_V(p_terrain == nullptr, Vector3());
	// The mesher is required to know how collisions should be processed
	ERR_FAIL_COND_V(p_terrain->get_mesher().is_null(), Vector3());

	// Transform to local in case the volume is transformed
	const Transform to_world = p_terrain->get_global_transform();
	const Transform to_local = to_world.affine_inverse();
	const Vector3 pos = to_local.xform(p_pos);
	const Vector3 motion = to_local.basis.xform(p_motion);
	const AABB aabb = Transform(to_local.basis, Vector3()).xform(p_aabb);

	const AABB box(aabb.position + pos, aabb.size);
	const AABB expanded_box = expand_with_vector(box, motion);

	static thread_local std::vector<AABB> s_colliding_boxes;
	std::vector<AABB> &potential_boxes = s_colliding_boxes;
	potential_boxes.clear();

	// Collect potential collisions with the terrain (broad phase)

	const VoxelDataMap &voxels = p_terrain->get_storage();

	const int min_x = int(Math::floor(expanded_box.position.x));
	const int min_y = int(Math::floor(expanded_box.position.y));
	const int min_z = int(Math::floor(expanded_box.position.z));

	const Vector3 expanded_box_end = expanded_box.position + expanded_box.size;
	const int max_x = int(Math::ceil(expanded_box_end.x));
	const int max_y = int(Math::ceil(expanded_box_end.y));
	const int max_z = int(Math::ceil(expanded_box_end.z));

	Vector3i i(min_x, min_y, min_z);

	Ref<VoxelMesherBlocky> mesher_blocky;
	Ref<VoxelMesherCubes> mesher_cubes;

	if (try_get_as(p_terrain->get_mesher(), mesher_blocky)) {
		Ref<VoxelLibrary> library_ref = mesher_blocky->get_library();
		ERR_FAIL_COND_V_MSG(library_ref.is_null(), Vector3(), "VoxelMesherBlocky has no library assigned");
		VoxelLibrary &library = **library_ref;
		const int channel = VoxelBuffer::CHANNEL_TYPE;

		for (i.z = min_z; i.z < max_z; ++i.z) {
			for (i.y = min_y; i.y < max_y; ++i.y) {
				for (i.x = min_x; i.x < max_x; ++i.x) {
					const int type_id = voxels.get_voxel(i, channel);

					if (library.has_voxel(type_id)) {
						const Voxel &voxel_type = library.get_voxel_const(type_id);

						if ((voxel_type.get_collision_mask() & _collision_mask) == 0) {
							continue;
						}

						const std::vector<AABB> &local_boxes = voxel_type.get_collision_aabbs();

						for (auto it = local_boxes.begin(); it != local_boxes.end(); ++it) {
							AABB world_box = *it;
							world_box.position += i.to_vec3();
							potential_boxes.push_back(world_box);
						}
					}
				}
			}
		}

	} else if (try_get_as(p_terrain->get_mesher(), mesher_cubes)) {
		const int channel = VoxelBuffer::CHANNEL_COLOR;

		for (i.z = min_z; i.z < max_z; ++i.z) {
			for (i.y = min_y; i.y < max_y; ++i.y) {
				for (i.x = min_x; i.x < max_x; ++i.x) {
					const int color_data = voxels.get_voxel(i, channel);
					if (color_data != 0) {
						potential_boxes.push_back(AABB(i.to_vec3(), Vector3(1, 1, 1)));
					}
				}
			}
		}
	}

	// Calculate collisions (narrow phase)
	const Vector3 slided_motion = ::get_motion(box, motion, potential_boxes);

	// Switch back to world
	const Vector3 world_slided_motion = to_world.basis.xform(slided_motion);

	return world_slided_motion;
}

void VoxelBoxMover::set_collision_mask(uint32_t mask) {
	_collision_mask = mask;
}

void VoxelBoxMover::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_motion", "pos", "motion", "aabb", "terrain"), &VoxelBoxMover::_b_get_motion);
	ClassDB::bind_method(D_METHOD("set_collision_mask", "mask"), &VoxelBoxMover::set_collision_mask);
	ClassDB::bind_method(D_METHOD("get_collision_mask"), &VoxelBoxMover::get_collision_mask);
}

Vector3 VoxelBoxMover::_b_get_motion(Vector3 pos, Vector3 motion, AABB aabb, Node *terrain_node) {
	ERR_FAIL_COND_V(terrain_node == nullptr, Vector3());
	VoxelTerrain *terrain = Object::cast_to<VoxelTerrain>(terrain_node);
	ERR_FAIL_COND_V(terrain == nullptr, Vector3());
	return get_motion(pos, motion, aabb, terrain);
}
