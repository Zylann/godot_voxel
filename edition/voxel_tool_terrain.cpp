#include "voxel_tool_terrain.h"
#include "../meshers/blocky/voxel_mesher_blocky.h"
#include "../meshers/cubes/voxel_mesher_cubes.h"
#include "../terrain/voxel_terrain.h"
#include "../util/godot/funcs.h"
#include "../util/voxel_raycast.h"

#include <core/func_ref.h>

VoxelToolTerrain::VoxelToolTerrain() {
}

VoxelToolTerrain::VoxelToolTerrain(VoxelTerrain *terrain) {
	ERR_FAIL_COND(terrain == nullptr);
	_terrain = terrain;
	// Don't destroy the terrain while a voxel tool still references it
}

bool VoxelToolTerrain::is_area_editable(const Box3i &box) const {
	ERR_FAIL_COND_V(_terrain == nullptr, false);
	// TODO Take volume bounds into account
	return _terrain->get_storage().is_area_fully_loaded(box);
}

Ref<VoxelRaycastResult> VoxelToolTerrain::raycast(Vector3 p_pos, Vector3 p_dir, float p_max_distance, uint32_t p_collision_mask) {
	// TODO Implement broad-phase on blocks to minimize locking and increase performance

	struct RaycastPredicateColor {
		const VoxelDataMap &map;

		bool operator()(const Vector3i pos) const {
			const uint64_t v = map.get_voxel(pos, VoxelBuffer::CHANNEL_COLOR);
			return v != 0;
		}
	};

	struct RaycastPredicateSDF {
		const VoxelDataMap &map;

		bool operator()(const Vector3i pos) const {
			const float v = map.get_voxel_f(pos, VoxelBuffer::CHANNEL_SDF);
			return v < 0;
		}
	};

	struct RaycastPredicateBlocky {
		const VoxelDataMap &map;
		const VoxelLibrary &library;
		const uint32_t collision_mask;

		bool operator()(const Vector3i pos) const {
			const int v = map.get_voxel(pos, VoxelBuffer::CHANNEL_TYPE);

			if (library.has_voxel(v) == false) {
				return false;
			}

			const Voxel &voxel = library.get_voxel_const(v);
			if (voxel.is_empty()) {
				return false;
			}

			if ((voxel.get_collision_mask() & collision_mask) == 0) {
				return false;
			}

			if (voxel.is_transparent() == false) {
				return true;
			}

			if (voxel.is_transparent() && voxel.get_collision_aabbs().empty() == false) {
				return true;
			}

			return false;
		}
	};

	Ref<VoxelRaycastResult> res;

	Ref<VoxelMesherBlocky> mesher_blocky;
	Ref<VoxelMesherCubes> mesher_cubes;

	Vector3i hit_pos;
	Vector3i prev_pos;

	const Transform to_world = _terrain->get_global_transform();
	const Transform to_local = to_world.affine_inverse();
	const Vector3 local_pos = to_local.xform(p_pos);
	const Vector3 local_dir = to_local.basis.xform(p_dir).normalized();
	const float to_world_scale = to_world.basis.get_axis(0).length();
	const float max_distance = p_max_distance / to_world_scale;

	if (try_get_as(_terrain->get_mesher(), mesher_blocky)) {
		Ref<VoxelLibrary> library_ref = mesher_blocky->get_library();
		if (library_ref.is_null()) {
			return res;
		}
		RaycastPredicateBlocky predicate{ _terrain->get_storage(), **library_ref, p_collision_mask };
		float hit_distance;
		float hit_distance_prev;
		if (voxel_raycast(local_pos, local_dir, predicate, max_distance, hit_pos, prev_pos, hit_distance, hit_distance_prev)) {
			res.instance();
			res->position = hit_pos;
			res->previous_position = prev_pos;
			res->distance_along_ray = hit_distance * to_world_scale;
		}

	} else if (try_get_as(_terrain->get_mesher(), mesher_cubes)) {
		RaycastPredicateColor predicate{ _terrain->get_storage() };
		float hit_distance;
		float hit_distance_prev;
		if (voxel_raycast(local_pos, local_dir, predicate, max_distance, hit_pos, prev_pos, hit_distance, hit_distance_prev)) {
			res.instance();
			res->position = hit_pos;
			res->previous_position = prev_pos;
			res->distance_along_ray = hit_distance * to_world_scale;
		}

	} else {
		RaycastPredicateSDF predicate{ _terrain->get_storage() };
		float hit_distance;
		float hit_distance_prev;
		if (voxel_raycast(local_pos, local_dir, predicate, max_distance, hit_pos, prev_pos, hit_distance, hit_distance_prev)) {
			res.instance();
			res->position = hit_pos;
			res->previous_position = prev_pos;
			res->distance_along_ray = hit_distance * to_world_scale;
		}
	}

	return res;
}

void VoxelToolTerrain::copy(Vector3i pos, Ref<VoxelBuffer> dst, uint8_t channels_mask) const {
	ERR_FAIL_COND(_terrain == nullptr);
	ERR_FAIL_COND(dst.is_null());
	if (channels_mask == 0) {
		channels_mask = (1 << _channel);
	}
	_terrain->get_storage().copy(pos, dst->get_buffer(), channels_mask);
}

void VoxelToolTerrain::paste(Vector3i pos, Ref<VoxelBuffer> p_voxels, uint8_t channels_mask, bool use_mask,
		uint64_t mask_value) {
	ERR_FAIL_COND(_terrain == nullptr);
	ERR_FAIL_COND(p_voxels.is_null());
	if (channels_mask == 0) {
		channels_mask = (1 << _channel);
	}
	_terrain->get_storage().paste(pos, p_voxels->get_buffer(), channels_mask, use_mask, mask_value, false);
	_post_edit(Box3i(pos, p_voxels->get_buffer().get_size()));
}

void VoxelToolTerrain::do_sphere(Vector3 center, float radius) {
	ERR_FAIL_COND(_terrain == nullptr);

	if (_mode != MODE_TEXTURE_PAINT) {
		VoxelTool::do_sphere(center, radius);
		return;
	}

	VOXEL_PROFILE_SCOPE();

	const Box3i box(Vector3i::from_floored(center) - Vector3i(Math::floor(radius)), Vector3i(Math::ceil(radius) * 2));

	if (!is_area_editable(box)) {
		PRINT_VERBOSE("Area not editable");
		return;
	}

	_terrain->get_storage().write_box_2(box, VoxelBuffer::CHANNEL_INDICES, VoxelBuffer::CHANNEL_WEIGHTS,
			TextureBlendSphereOp{ center, radius, _texture_params });

	_post_edit(box);
}

uint64_t VoxelToolTerrain::_get_voxel(Vector3i pos) const {
	ERR_FAIL_COND_V(_terrain == nullptr, 0);
	return _terrain->get_storage().get_voxel(pos, _channel);
}

float VoxelToolTerrain::_get_voxel_f(Vector3i pos) const {
	ERR_FAIL_COND_V(_terrain == nullptr, 0);
	return _terrain->get_storage().get_voxel_f(pos, _channel);
}

void VoxelToolTerrain::_set_voxel(Vector3i pos, uint64_t v) {
	ERR_FAIL_COND(_terrain == nullptr);
	_terrain->get_storage().set_voxel(v, pos, _channel);
}

void VoxelToolTerrain::_set_voxel_f(Vector3i pos, float v) {
	ERR_FAIL_COND(_terrain == nullptr);
	_terrain->get_storage().set_voxel_f(v, pos, _channel);
}

void VoxelToolTerrain::_post_edit(const Box3i &box) {
	ERR_FAIL_COND(_terrain == nullptr);
	_terrain->post_edit_area(box);
}

void VoxelToolTerrain::set_voxel_metadata(Vector3i pos, Variant meta) {
	ERR_FAIL_COND(_terrain == nullptr);
	VoxelDataMap &map = _terrain->get_storage();
	VoxelDataBlock *block = map.get_block(map.voxel_to_block(pos));
	ERR_FAIL_COND_MSG(block == nullptr, "Area not editable");
	RWLockWrite lock(block->get_voxels().get_lock());
	block->get_voxels().set_voxel_metadata(map.to_local(pos), meta);
}

Variant VoxelToolTerrain::get_voxel_metadata(Vector3i pos) const {
	ERR_FAIL_COND_V(_terrain == nullptr, Variant());
	VoxelDataMap &map = _terrain->get_storage();
	VoxelDataBlock *block = map.get_block(map.voxel_to_block(pos));
	ERR_FAIL_COND_V_MSG(block == nullptr, Variant(), "Area not editable");
	RWLockRead lock(block->get_voxels().get_lock());
	return block->get_voxels_const().get_voxel_metadata(map.to_local(pos));
}

// Executes a function on random voxels in the provided area, using the type channel.
// This allows to implement slow "natural" cellular automata behavior, as can be seen in Minecraft.
void VoxelToolTerrain::run_blocky_random_tick(AABB voxel_area, int voxel_count, Ref<FuncRef> callback, int batch_count) const {
	VOXEL_PROFILE_SCOPE();

	ERR_FAIL_COND(_terrain == nullptr);
	ERR_FAIL_COND_MSG(_terrain->get_voxel_library().is_null(),
			"This function requires a volume using VoxelMesherBlocky");
	ERR_FAIL_COND(callback.is_null());
	ERR_FAIL_COND(batch_count <= 0);
	ERR_FAIL_COND(voxel_count < 0);
	ERR_FAIL_COND(!is_valid_size(voxel_area.size));

	if (voxel_count == 0) {
		return;
	}

	const VoxelLibrary &lib = **_terrain->get_voxel_library();

	const Vector3i min_pos = Vector3i::from_floored(voxel_area.position);
	const Vector3i max_pos = min_pos + Vector3i::from_floored(voxel_area.size);

	VoxelDataMap &map = _terrain->get_storage();

	const Vector3i min_block_pos = map.voxel_to_block(min_pos);
	const Vector3i max_block_pos = map.voxel_to_block(max_pos);
	const Vector3i block_area_size = max_block_pos - min_block_pos;

	const int block_count = voxel_count / batch_count;
	const int bs_mask = map.get_block_size_mask();
	const VoxelBufferInternal::ChannelId channel = VoxelBufferInternal::CHANNEL_TYPE;

	struct Pick {
		uint64_t value;
		Vector3i rpos;
	};
	std::vector<Pick> picks;
	picks.resize(batch_count);

	// Choose blocks at random
	for (int bi = 0; bi < block_count; ++bi) {
		const Vector3i block_pos = min_block_pos + Vector3i(
														   Math::rand() % block_area_size.x,
														   Math::rand() % block_area_size.y,
														   Math::rand() % block_area_size.z);

		const Vector3i block_origin = map.block_to_voxel(block_pos);

		VoxelDataBlock *block = map.get_block(block_pos);
		if (block != nullptr) {
			// Doing ONLY reads here.
			{
				RWLockRead lock(block->get_voxels().get_lock());
				const VoxelBufferInternal &voxels = block->get_voxels_const();

				if (voxels.get_channel_compression(channel) == VoxelBufferInternal::COMPRESSION_UNIFORM) {
					const uint64_t v = voxels.get_voxel(0, 0, 0, channel);
					if (lib.has_voxel(v)) {
						const Voxel &vt = lib.get_voxel_const(v);
						if (!vt.is_random_tickable()) {
							// Skip whole block
							continue;
						}
					}
				}

				// Choose a bunch of voxels at random within the block.
				// Batching this way improves performance a little by reducing block lookups.
				for (int vi = 0; vi < batch_count; ++vi) {
					const Vector3i rpos(
							Math::rand() & bs_mask,
							Math::rand() & bs_mask,
							Math::rand() & bs_mask);

					const uint64_t v = voxels.get_voxel(rpos, channel);
					picks[vi] = Pick{ v, rpos };
				}
			}

			// The following may or may not read AND write voxels randomly due to its exposition to scripts.
			// However, we don't send the buffer directly, so it will go through an API taking care of locking.
			// So we don't (and shouldn't) lock anything here.
			for (size_t i = 0; i < picks.size(); ++i) {
				const Pick pick = picks[i];

				if (lib.has_voxel(pick.value)) {
					const Voxel &vt = lib.get_voxel_const(pick.value);

					if (vt.is_random_tickable()) {
						const Variant vpos = (pick.rpos + block_origin).to_vec3();
						const Variant vv = pick.value;
						const Variant *args[2];
						args[0] = &vpos;
						args[1] = &vv;
						Variant::CallError error;
						callback->call_func(args, 2, error);
						// TODO I would really like to know what's the correct way to report such errors...
						// Examples I found in the engine are inconsistent
						ERR_FAIL_COND(error.error != Variant::CallError::CALL_OK);
						// Return if it fails, we don't want an error spam
					}
				}
			}
		}
	}
}

void VoxelToolTerrain::for_each_voxel_metadata_in_area(AABB voxel_area, Ref<FuncRef> callback) {
	ERR_FAIL_COND(_terrain == nullptr);
	ERR_FAIL_COND(callback.is_null());
	ERR_FAIL_COND(!is_valid_size(voxel_area.size));

	const Box3i voxel_box = Box3i(Vector3i::from_floored(voxel_area.position), Vector3i::from_floored(voxel_area.size));
	ERR_FAIL_COND(!is_area_editable(voxel_box));

	const Box3i data_block_box = voxel_box.downscaled(_terrain->get_data_block_size());

	VoxelDataMap &map = _terrain->get_storage();

	data_block_box.for_each_cell([&map, &callback, voxel_box](Vector3i block_pos) {
		VoxelDataBlock *block = map.get_block(block_pos);

		if (block == nullptr) {
			return;
		}

		const Vector3i block_origin = block_pos * map.get_block_size();
		const Box3i rel_voxel_box(voxel_box.pos - block_origin, voxel_box.size);
		// TODO Worth it locking blocks for metadata?

		block->get_voxels().for_each_voxel_metadata_in_area(rel_voxel_box, [&callback, block_origin](Vector3i rel_pos, Variant meta) {
			const Variant key = (rel_pos + block_origin).to_vec3();
			const Variant *args[2] = { &key, &meta };
			Variant::CallError err;
			callback->call_func(args, 2, err);

			ERR_FAIL_COND_MSG(err.error != Variant::CallError::CALL_OK,
					String("FuncRef call failed at {0}").format(varray(key)));

			// TODO Can't provide detailed error because FuncRef doesn't give us access to the object
			// ERR_FAIL_COND_MSG(err.error != Variant::CallError::CALL_OK, false,
			// 		Variant::get_call_error_text(callback->get_object(), method_name, nullptr, 0, err));
		});
	});
}

void VoxelToolTerrain::_bind_methods() {
	ClassDB::bind_method(D_METHOD("run_blocky_random_tick", "area", "voxel_count", "callback", "batch_count"),
			&VoxelToolTerrain::run_blocky_random_tick, DEFVAL(16));
	ClassDB::bind_method(D_METHOD("for_each_voxel_metadata_in_area", "voxel_area", "callback"),
			&VoxelToolTerrain::for_each_voxel_metadata_in_area);
}
