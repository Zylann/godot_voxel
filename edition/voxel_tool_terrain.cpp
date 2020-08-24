#include "voxel_tool_terrain.h"
#include "../terrain/voxel_map.h"
#include "../terrain/voxel_terrain.h"
#include "../util/voxel_raycast.h"
#include <core/func_ref.h>

VoxelToolTerrain::VoxelToolTerrain() {
}

VoxelToolTerrain::VoxelToolTerrain(VoxelTerrain *terrain, Ref<VoxelMap> map) {
	ERR_FAIL_COND(terrain == nullptr);
	_terrain = terrain;
	_map = map;
	// Don't destroy the terrain while a voxel tool still references it
}

bool VoxelToolTerrain::is_area_editable(const Rect3i &box) const {
	ERR_FAIL_COND_V(_terrain == nullptr, false);
	return _map->is_area_fully_loaded(box.padded(1));
}

Ref<VoxelRaycastResult> VoxelToolTerrain::raycast(Vector3 pos, Vector3 dir, float max_distance, uint32_t collision_mask) {
	// TODO Transform input if the terrain is rotated (in the future it can be made a Spatial node)
	// TODO Optimize using a broad phase on blocks

	struct RaycastPredicate {
		const VoxelTerrain &terrain;
		const uint32_t collision_mask;

		bool operator()(Vector3i pos) {
			//unsigned int channel = context->channel;

			Ref<VoxelMap> map = terrain.get_storage();
			int v0 = map->get_voxel(pos, VoxelBuffer::CHANNEL_TYPE);

			Ref<VoxelLibrary> lib_ref = terrain.get_voxel_library();
			if (lib_ref.is_null()) {
				return false;
			}
			const VoxelLibrary &lib = **lib_ref;

			if (lib.has_voxel(v0) == false) {
				return false;
			}

			const Voxel &voxel = lib.get_voxel_const(v0);
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

			float v1 = map->get_voxel_f(pos, VoxelBuffer::CHANNEL_SDF);
			return v1 < 0;
		}
	};

	Vector3i hit_pos;
	Vector3i prev_pos;
	Ref<VoxelRaycastResult> res;

	RaycastPredicate predicate = { *_terrain, collision_mask };
	if (voxel_raycast(pos, dir, predicate, max_distance, hit_pos, prev_pos)) {
		res.instance();
		res->position = hit_pos;
		res->previous_position = prev_pos;
	}

	return res;
}

uint64_t VoxelToolTerrain::_get_voxel(Vector3i pos) {
	ERR_FAIL_COND_V(_terrain == nullptr, 0);
	return _map->get_voxel(pos, _channel);
}

float VoxelToolTerrain::_get_voxel_f(Vector3i pos) {
	ERR_FAIL_COND_V(_terrain == nullptr, 0);
	return _map->get_voxel_f(pos, _channel);
}

void VoxelToolTerrain::_set_voxel(Vector3i pos, uint64_t v) {
	ERR_FAIL_COND(_terrain == nullptr);
	_map->set_voxel(v, pos, _channel);
}

void VoxelToolTerrain::_set_voxel_f(Vector3i pos, float v) {
	ERR_FAIL_COND(_terrain == nullptr);
	_map->set_voxel_f(v, pos, _channel);
}

void VoxelToolTerrain::_post_edit(const Rect3i &box) {
	ERR_FAIL_COND(_terrain == nullptr);
	_terrain->make_area_dirty(box);
}

void VoxelToolTerrain::set_voxel_metadata(Vector3i pos, Variant meta) {
	ERR_FAIL_COND(_terrain == nullptr);
	VoxelBlock *block = _map->get_block(_map->voxel_to_block(pos));
	ERR_FAIL_COND_MSG(block == nullptr, "Area not editable");
	block->voxels->set_voxel_metadata(_map->to_local(pos), meta);
}

Variant VoxelToolTerrain::get_voxel_metadata(Vector3i pos) {
	ERR_FAIL_COND_V(_terrain == nullptr, Variant());
	const VoxelBlock *block = _map->get_block(_map->voxel_to_block(pos));
	ERR_FAIL_COND_V_MSG(block == nullptr, Variant(), "Area not editable");
	return block->voxels->get_voxel_metadata(_map->to_local(pos));
}

// Executes a function on random voxels in the provided area, using the type channel.
// This allows to implement slow "natural" cellular automata behavior, as can be seen in Minecraft.
void VoxelToolTerrain::run_blocky_random_tick(AABB voxel_area, int voxel_count, Ref<FuncRef> callback, int batch_count) const {
	ERR_FAIL_COND(_terrain == nullptr);
	ERR_FAIL_COND(_terrain->get_voxel_library().is_null());
	ERR_FAIL_COND(callback.is_null());
	ERR_FAIL_COND(batch_count <= 0);
	ERR_FAIL_COND(voxel_count < 0);

	if (voxel_count == 0) {
		return;
	}

	const VoxelLibrary &lib = **_terrain->get_voxel_library();

	const Vector3i min_pos = Vector3i(voxel_area.position);
	const Vector3i max_pos = min_pos + Vector3i(voxel_area.size);

	const Vector3i min_block_pos = _map->voxel_to_block(min_pos);
	const Vector3i max_block_pos = _map->voxel_to_block(max_pos);
	const Vector3i block_area_size = max_block_pos - min_block_pos;

	const int block_count = voxel_count / batch_count;
	const int bs_mask = _map->get_block_size_mask();
	const VoxelBuffer::ChannelId channel = VoxelBuffer::CHANNEL_TYPE;

	// Choose blocks at random
	for (int bi = 0; bi < block_count; ++bi) {
		const Vector3i block_pos = min_block_pos + Vector3i(
														   Math::rand() % block_area_size.x,
														   Math::rand() % block_area_size.y,
														   Math::rand() % block_area_size.z);
		const Vector3i block_origin = _map->block_to_voxel(block_pos);

		const VoxelBlock *block = _map->get_block(block_pos);
		if (block != nullptr) {
			if (block->voxels->get_channel_compression(channel) == VoxelBuffer::COMPRESSION_UNIFORM) {
				const uint64_t v = block->voxels->get_voxel(0, 0, 0, channel);
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

				const uint64_t v = block->voxels->get_voxel(rpos, channel);

				if (lib.has_voxel(v)) {
					const Voxel &vt = lib.get_voxel_const(v);

					if (vt.is_random_tickable()) {
						const Variant vpos = (rpos + block_origin).to_vec3();
						const Variant vv = v;
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

void VoxelToolTerrain::_bind_methods() {
	ClassDB::bind_method(D_METHOD("run_blocky_random_tick", "voxel_count", "callback", "batch_count"),
			&VoxelToolTerrain::run_blocky_random_tick, DEFVAL(16));
}
