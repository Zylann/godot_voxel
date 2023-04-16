#include "voxel_tool_terrain.h"
#include "../meshers/blocky/voxel_mesher_blocky.h"
#include "../meshers/cubes/voxel_mesher_cubes.h"
#include "../storage/voxel_buffer_gd.h"
#include "../storage/voxel_data.h"
#include "../storage/voxel_metadata_variant.h"
#include "../terrain/fixed_lod/voxel_terrain.h"
#include "../util/godot/classes/ref_counted.h"
#include "../util/godot/core/array.h"
#include "../util/math/conv.h"
#include "../util/voxel_raycast.h"

namespace zylann::voxel {

VoxelToolTerrain::VoxelToolTerrain() {
	_random.randomize();
}

VoxelToolTerrain::VoxelToolTerrain(VoxelTerrain *terrain) {
	ERR_FAIL_COND(terrain == nullptr);
	_terrain = terrain;
	// Don't destroy the terrain while a voxel tool still references it
}

bool VoxelToolTerrain::is_area_editable(const Box3i &box) const {
	ERR_FAIL_COND_V(_terrain == nullptr, false);
	// TODO Take volume bounds into account
	return _terrain->get_storage().is_area_loaded(box);
}

Ref<VoxelRaycastResult> VoxelToolTerrain::raycast(
		Vector3 p_pos, Vector3 p_dir, float p_max_distance, uint32_t p_collision_mask) {
	// TODO Implement broad-phase on blocks to minimize locking and increase performance

	// TODO Optimization: voxel raycast uses `get_voxel` which is the slowest, but could be made faster.
	// See `VoxelToolLodTerrain` for information about how to implement improvements.

	struct RaycastPredicateColor {
		const VoxelData &data;

		bool operator()(const VoxelRaycastState &rs) const {
			VoxelSingleValue defval;
			defval.i = 0;
			const uint64_t v = data.get_voxel(rs.hit_position, VoxelBufferInternal::CHANNEL_COLOR, defval).i;
			return v != 0;
		}
	};

	struct RaycastPredicateSDF {
		const VoxelData &data;

		bool operator()(const VoxelRaycastState &rs) const {
			const float v = data.get_voxel_f(rs.hit_position, VoxelBufferInternal::CHANNEL_SDF);
			return v < 0;
		}
	};

	struct RaycastPredicateBlocky {
		const VoxelData &data;
		const VoxelBlockyLibrary &library;
		const uint32_t collision_mask;

		bool operator()(const VoxelRaycastState &rs) const {
			VoxelSingleValue defval;
			defval.i = 0;
			const int v = data.get_voxel(rs.hit_position, VoxelBufferInternal::CHANNEL_TYPE, defval).i;

			if (library.has_voxel(v) == false) {
				return false;
			}

			const VoxelBlockyModel &voxel = library.get_voxel_const(v);
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

	const Transform3D to_world = _terrain->get_global_transform();
	const Transform3D to_local = to_world.affine_inverse();
	const Vector3 local_pos = to_local.xform(p_pos);
	const Vector3 local_dir = to_local.basis.xform(p_dir).normalized();
	const float to_world_scale = to_world.basis.get_column(Vector3::AXIS_X).length();
	const float max_distance = p_max_distance / to_world_scale;

	if (try_get_as(_terrain->get_mesher(), mesher_blocky)) {
		Ref<VoxelBlockyLibrary> library_ref = mesher_blocky->get_library();
		if (library_ref.is_null()) {
			return res;
		}
		RaycastPredicateBlocky predicate{ _terrain->get_storage(), **library_ref, p_collision_mask };
		float hit_distance;
		float hit_distance_prev;
		if (zylann::voxel_raycast(local_pos, local_dir, predicate, max_distance, hit_pos, prev_pos, hit_distance,
					hit_distance_prev)) {
			res.instantiate();
			res->position = hit_pos;
			res->previous_position = prev_pos;
			res->distance_along_ray = hit_distance * to_world_scale;
		}

	} else if (try_get_as(_terrain->get_mesher(), mesher_cubes)) {
		RaycastPredicateColor predicate{ _terrain->get_storage() };
		float hit_distance;
		float hit_distance_prev;
		if (zylann::voxel_raycast(local_pos, local_dir, predicate, max_distance, hit_pos, prev_pos, hit_distance,
					hit_distance_prev)) {
			res.instantiate();
			res->position = hit_pos;
			res->previous_position = prev_pos;
			res->distance_along_ray = hit_distance * to_world_scale;
		}

	} else {
		RaycastPredicateSDF predicate{ _terrain->get_storage() };
		float hit_distance;
		float hit_distance_prev;
		if (zylann::voxel_raycast(local_pos, local_dir, predicate, max_distance, hit_pos, prev_pos, hit_distance,
					hit_distance_prev)) {
			res.instantiate();
			res->position = hit_pos;
			res->previous_position = prev_pos;
			res->distance_along_ray = hit_distance * to_world_scale;
		}
	}

	return res;
}

void VoxelToolTerrain::copy(Vector3i pos, Ref<gd::VoxelBuffer> dst, uint8_t channels_mask) const {
	ERR_FAIL_COND(_terrain == nullptr);
	ERR_FAIL_COND(dst.is_null());
	if (channels_mask == 0) {
		channels_mask = (1 << _channel);
	}
	_terrain->get_storage().copy(pos, dst->get_buffer(), channels_mask);
}

void VoxelToolTerrain::paste(Vector3i pos, Ref<gd::VoxelBuffer> p_voxels, uint8_t channels_mask) {
	ERR_FAIL_COND(_terrain == nullptr);
	ERR_FAIL_COND(p_voxels.is_null());
	if (channels_mask == 0) {
		channels_mask = (1 << _channel);
	}
	_terrain->get_storage().paste(pos, p_voxels->get_buffer(), channels_mask, false);
	_post_edit(Box3i(pos, p_voxels->get_buffer().get_size()));
}

void VoxelToolTerrain::paste_masked(
		Vector3i pos, Ref<gd::VoxelBuffer> p_voxels, uint8_t channels_mask, uint8_t mask_channel, uint64_t mask_value) {
	ERR_FAIL_COND(_terrain == nullptr);
	ERR_FAIL_COND(p_voxels.is_null());
	if (channels_mask == 0) {
		channels_mask = (1 << _channel);
	}
	_terrain->get_storage().paste_masked(pos, p_voxels->get_buffer(), channels_mask, mask_channel, mask_value, false);
	_post_edit(Box3i(pos, p_voxels->get_buffer().get_size()));
}

void VoxelToolTerrain::do_sphere(Vector3 center, float radius) {
	ZN_PROFILE_SCOPE();
	ERR_FAIL_COND(_terrain == nullptr);

	ops::DoSphere op;
	op.shape.center = center;
	op.shape.radius = radius;
	op.shape.sdf_scale = get_sdf_scale();
	op.box = op.shape.get_box().clipped(_terrain->get_bounds());
	op.mode = ops::Mode(get_mode());
	op.texture_params = _texture_params;
	op.blocky_value = _value;
	op.channel = get_channel();
	op.strength = get_sdf_strength();

	if (!is_area_editable(op.box)) {
		ZN_PRINT_VERBOSE("Area not editable");
		return;
	}

	VoxelData &data = _terrain->get_storage();

	data.get_blocks_grid(op.blocks, op.box, 0);
	op();

	_post_edit(op.box);
}

void VoxelToolTerrain::do_hemisphere(Vector3 center, float radius, Vector3 flat_direction, float smoothness) {
	ZN_PROFILE_SCOPE();
	ERR_FAIL_COND(_terrain == nullptr);

	ops::DoHemisphere op;
	op.shape.center = center;
	op.shape.radius = radius;
	op.shape.flat_direction = flat_direction;
	op.shape.plane_d = flat_direction.dot(center);
	op.shape.smoothness = smoothness;
	op.shape.sdf_scale = get_sdf_scale();
	op.box = op.shape.get_box().clipped(_terrain->get_bounds());
	op.mode = ops::Mode(get_mode());
	op.texture_params = _texture_params;
	op.blocky_value = _value;
	op.channel = get_channel();
	op.strength = get_sdf_strength();

	if (!is_area_editable(op.box)) {
		ZN_PRINT_VERBOSE("Area not editable");
		return;
	}

	VoxelData &data = _terrain->get_storage();

	data.get_blocks_grid(op.blocks, op.box, 0);
	op();

	_post_edit(op.box);
}

uint64_t VoxelToolTerrain::_get_voxel(Vector3i pos) const {
	ERR_FAIL_COND_V(_terrain == nullptr, 0);
	VoxelSingleValue defval;
	defval.i = 0;
	return _terrain->get_storage().get_voxel(pos, _channel, defval).i;
}

float VoxelToolTerrain::_get_voxel_f(Vector3i pos) const {
	ERR_FAIL_COND_V(_terrain == nullptr, 0);
	return _terrain->get_storage().get_voxel_f(pos, _channel);
}

void VoxelToolTerrain::_set_voxel(Vector3i pos, uint64_t v) {
	ERR_FAIL_COND(_terrain == nullptr);
	_terrain->get_storage().try_set_voxel(v, pos, _channel);
}

void VoxelToolTerrain::_set_voxel_f(Vector3i pos, float v) {
	ERR_FAIL_COND(_terrain == nullptr);
	_terrain->get_storage().try_set_voxel_f(v, pos, _channel);
}

void VoxelToolTerrain::_post_edit(const Box3i &box) {
	ERR_FAIL_COND(_terrain == nullptr);
	_terrain->post_edit_area(box);
}

void VoxelToolTerrain::set_voxel_metadata(Vector3i pos, Variant meta) {
	ERR_FAIL_COND(_terrain == nullptr);
	VoxelData &data = _terrain->get_storage();
	data.set_voxel_metadata(pos, meta);
}

Variant VoxelToolTerrain::get_voxel_metadata(Vector3i pos) const {
	ERR_FAIL_COND_V(_terrain == nullptr, Variant());
	VoxelData &data = _terrain->get_storage();
	return data.get_voxel_metadata(pos);
}

void VoxelToolTerrain::run_blocky_random_tick_static(VoxelData &data, Box3i voxel_box, const VoxelBlockyLibrary &lib,
		RandomPCG &random, int voxel_count, int batch_count, void *callback_data,
		bool (*callback)(void *, Vector3i, int64_t)) {
	ERR_FAIL_COND(batch_count <= 0);
	ERR_FAIL_COND(voxel_count < 0);
	ERR_FAIL_COND(!math::is_valid_size(voxel_box.size));
	ERR_FAIL_COND(callback == nullptr);

	const unsigned int block_size = data.get_block_size();
	const Box3i block_box = voxel_box.downscaled(block_size);

	const int block_count = voxel_count / batch_count;
	// const int bs_mask = map.get_block_size_mask();
	const VoxelBufferInternal::ChannelId channel = VoxelBufferInternal::CHANNEL_TYPE;

	struct Pick {
		uint64_t value;
		Vector3i rpos;
	};
	static thread_local std::vector<Pick> picks;
	picks.reserve(batch_count);

	const float block_volume = math::cubed(block_size);
	CRASH_COND(block_volume < 0.1f);

	struct L {
		static inline int urand(RandomPCG &random, uint32_t max_value) {
			return random.rand() % max_value;
		}
		static inline Vector3i urand_vec3i(RandomPCG &random, Vector3i s) {
#ifdef DEBUG_ENABLED
			CRASH_COND(s.x <= 0 || s.y <= 0 || s.z <= 0);
#endif
			return Vector3i(urand(random, s.x), urand(random, s.y), urand(random, s.z));
		}
	};

	// Choose blocks at random
	for (int bi = 0; bi < block_count; ++bi) {
		const Vector3i block_pos = block_box.pos + L::urand_vec3i(random, block_box.size);

		const Vector3i block_origin = data.block_to_voxel(block_pos);

		std::shared_ptr<VoxelBufferInternal> voxels_ptr = data.try_get_block_voxels(block_pos);

		if (voxels_ptr != nullptr) {
			// Doing ONLY reads here.
			{
				RWLockRead lock(voxels_ptr->get_lock());
				const VoxelBufferInternal &voxels = *voxels_ptr;

				if (voxels.get_channel_compression(channel) == VoxelBufferInternal::COMPRESSION_UNIFORM) {
					const uint64_t v = voxels.get_voxel(0, 0, 0, channel);
					if (lib.has_voxel(v)) {
						const VoxelBlockyModel &vt = lib.get_voxel_const(v);
						if (!vt.is_random_tickable()) {
							// Skip whole block
							continue;
						}
					}
				}

				const Box3i block_voxel_box(block_origin, Vector3iUtil::create(block_size));
				Box3i local_voxel_box = voxel_box.clipped(block_voxel_box);
				local_voxel_box.pos -= block_origin;
				const float volume_ratio = Vector3iUtil::get_volume(local_voxel_box.size) / block_volume;
				const int local_batch_count = Math::ceil(batch_count * volume_ratio);

				// Choose a bunch of voxels at random within the block.
				// Batching this way improves performance a little by reducing block lookups.
				picks.clear();
				for (int vi = 0; vi < local_batch_count; ++vi) {
					const Vector3i rpos = local_voxel_box.pos + L::urand_vec3i(random, local_voxel_box.size);

					const uint64_t v = voxels.get_voxel(rpos, channel);
					picks.push_back(Pick{ v, rpos });
				}
			}

			// The following may or may not read AND write voxels randomly due to its exposition to scripts.
			// However, we don't send the buffer directly, so it will go through an API taking care of locking.
			// So we don't (and shouldn't) lock anything here.
			for (size_t i = 0; i < picks.size(); ++i) {
				const Pick pick = picks[i];

				if (lib.has_voxel(pick.value)) {
					const VoxelBlockyModel &vt = lib.get_voxel_const(pick.value);

					if (vt.is_random_tickable()) {
						ERR_FAIL_COND(!callback(callback_data, pick.rpos + block_origin, pick.value));
					}
				}
			}
		}
	}
}

static Ref<VoxelBlockyLibrary> get_voxel_library(const VoxelTerrain &terrain) {
	Ref<VoxelMesherBlocky> blocky_mesher = terrain.get_mesher();
	if (blocky_mesher.is_valid()) {
		return blocky_mesher->get_library();
	}
	return Ref<VoxelBlockyLibrary>();
}

// TODO This function snaps the given AABB to blocks, this is not intuitive. Should figure out a way to respect the
// area. Executes a function on random voxels in the provided area, using the type channel. This allows to implement
// slow "natural" cellular automata behavior, as can be seen in Minecraft.
void VoxelToolTerrain::run_blocky_random_tick(
		AABB voxel_area, int voxel_count, const Callable &callback, int batch_count) {
	ZN_PROFILE_SCOPE();

	ERR_FAIL_COND(_terrain == nullptr);
	ERR_FAIL_COND_MSG(get_voxel_library(*_terrain).is_null(),
			String("This function requires a volume using {0}").format(varray(VoxelMesherBlocky::get_class_static())));
	ERR_FAIL_COND(callback.is_null());
	ERR_FAIL_COND(batch_count <= 0);
	ERR_FAIL_COND(voxel_count < 0);
	ERR_FAIL_COND(!math::is_valid_size(voxel_area.size));

	if (voxel_count == 0) {
		return;
	}

	struct CallbackData {
		const Callable &callable;
	};
	CallbackData cb_self{ callback };

	const VoxelBlockyLibrary &lib = **get_voxel_library(*_terrain);
	VoxelData &data = _terrain->get_storage();
	const Box3i voxel_box(math::floor_to_int(voxel_area.position), math::floor_to_int(voxel_area.size));

#if defined(ZN_GODOT)
	run_blocky_random_tick_static(data, voxel_box, lib, _random, voxel_count, batch_count, &cb_self,
			[](void *self, Vector3i pos, int64_t val) {
				const Variant vpos = pos;
				const Variant vv = val;
				const Variant *args[2];
				args[0] = &vpos;
				args[1] = &vv;
				Callable::CallError error;
				Variant retval; // We don't care about the return value, Callable API requires it
				const CallbackData *cd = (const CallbackData *)self;
				cd->callable.callp(args, 2, retval, error);
				// TODO I would really like to know what's the correct way to report such errors...
				// Examples I found in the engine are inconsistent
				ERR_FAIL_COND_V(error.error != Callable::CallError::CALL_OK, false);
				// Return if it fails, we don't want an error spam
				return true;
			});
#elif defined(ZN_GODOT_EXTENSION)
	// TODO GDX: Can't call Callables
	ZN_PRINT_ERROR("VoxelToolTerrain::run_blocky_random_tick isn't supported in GDExtension, cannot call Callables");
#endif
}

void VoxelToolTerrain::for_each_voxel_metadata_in_area(AABB voxel_area, const Callable &callback) {
	ERR_FAIL_COND(_terrain == nullptr);
	ERR_FAIL_COND(callback.is_null());
	ERR_FAIL_COND(!math::is_valid_size(voxel_area.size));

	const Box3i voxel_box = Box3i(math::floor_to_int(voxel_area.position), math::floor_to_int(voxel_area.size));
	ERR_FAIL_COND(!is_area_editable(voxel_box));

	const Box3i data_block_box = voxel_box.downscaled(_terrain->get_data_block_size());

	VoxelData &data = _terrain->get_storage();

	data_block_box.for_each_cell([&data, &callback, voxel_box](Vector3i block_pos) {
		std::shared_ptr<VoxelBufferInternal> voxels_ptr = data.try_get_block_voxels(block_pos);

		if (voxels_ptr == nullptr) {
			return;
		}

		const Vector3i block_origin = block_pos * data.get_block_size();
		const Box3i rel_voxel_box(voxel_box.pos - block_origin, voxel_box.size);
		// TODO Worth it locking blocks for metadata?

#if defined(ZN_GODOT)
		voxels_ptr->for_each_voxel_metadata_in_area(
				rel_voxel_box, [&callback, block_origin](Vector3i rel_pos, const VoxelMetadata &meta) {
					Variant v = gd::get_as_variant(meta);
					const Variant key = rel_pos + block_origin;
					const Variant *args[2] = { &key, &v };
					Callable::CallError err;
					Variant retval; // We don't care about the return value, Callable API requires it
					callback.callp(args, 2, retval, err);

					ERR_FAIL_COND_MSG(err.error != Callable::CallError::CALL_OK,
							String("Callable failed at {0}").format(varray(key)));

					// TODO Can't provide detailed error because FuncRef doesn't give us access to the object
					// ERR_FAIL_COND_MSG(err.error != Variant::CallError::CALL_OK, false,
					// 		Variant::get_call_error_text(callback->get_object(), method_name, nullptr, 0, err));
				});
#elif defined(ZN_GODOT_EXTENSION)
		// TODO GDX: Can't call Callables
		ZN_PRINT_ERROR("VoxelToolTerrain::for_each_voxel_metadata_in_area isn't supported in GDExtension, cannot call "
					   "Callables");
#endif
	});
}

void VoxelToolTerrain::_bind_methods() {
	ClassDB::bind_method(D_METHOD("run_blocky_random_tick", "area", "voxel_count", "callback", "batch_count"),
			&VoxelToolTerrain::run_blocky_random_tick, DEFVAL(16));
	ClassDB::bind_method(D_METHOD("for_each_voxel_metadata_in_area", "voxel_area", "callback"),
			&VoxelToolTerrain::for_each_voxel_metadata_in_area);
	ClassDB::bind_method(D_METHOD("do_hemisphere", "center", "radius", "flat_direction", "smoothness"),
			&VoxelToolTerrain::do_hemisphere, DEFVAL(0.0));
}

} // namespace zylann::voxel
