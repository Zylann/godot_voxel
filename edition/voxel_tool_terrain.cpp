#include "voxel_tool_terrain.h"
#include "../meshers/blocky/voxel_mesher_blocky.h"
#include "../meshers/cubes/voxel_mesher_cubes.h"
#include "../storage/metadata/voxel_metadata_variant.h"
#include "../storage/voxel_buffer_gd.h"
#include "../storage/voxel_data.h"
#include "../terrain/fixed_lod/voxel_terrain.h"
#include "../util/godot/classes/ref_counted.h"
#include "../util/godot/core/array.h"
#include "../util/godot/core/packed_arrays.h"
#include "../util/math/conv.h"
#include "../util/voxel_raycast.h"

using namespace zylann::godot;

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
		Vector3 p_pos,
		Vector3 p_dir,
		float p_max_distance,
		uint32_t p_collision_mask
) {
	// TODO Implement broad-phase on blocks to minimize locking and increase performance

	// TODO Optimization: voxel raycast uses `get_voxel` which is the slowest, but could be made faster.
	// See `VoxelToolLodTerrain` for information about how to implement improvements.

	struct RaycastPredicateColor {
		const VoxelData &data;

		bool operator()(const VoxelRaycastState &rs) const {
			VoxelSingleValue defval;
			defval.i = 0;
			const uint64_t v = data.get_voxel(rs.hit_position, VoxelBuffer::CHANNEL_COLOR, defval).i;
			return v != 0;
		}
	};

	struct RaycastPredicateSDF {
		const VoxelData &data;

		bool operator()(const VoxelRaycastState &rs) const {
			const float v = data.get_voxel_f(rs.hit_position, VoxelBuffer::CHANNEL_SDF);
			return v < 0;
		}
	};

	struct RaycastPredicateBlocky {
		const VoxelData &data;
		const VoxelBlockyLibraryBase::BakedData &baked_data;
		const uint32_t collision_mask;
		const Vector3 p_from;
		const Vector3 p_to;

		bool operator()(const VoxelRaycastState &rs) const {
			VoxelSingleValue defval;
			defval.i = 0;
			const int v = data.get_voxel(rs.hit_position, VoxelBuffer::CHANNEL_TYPE, defval).i;

			if (baked_data.has_model(v) == false) {
				return false;
			}

			const VoxelBlockyModel::BakedData &model = baked_data.models[v];
			if ((model.box_collision_mask & collision_mask) == 0) {
				return false;
			}

			for (const AABB &aabb : model.box_collision_aabbs) {
				if (AABB(aabb.position + rs.hit_position, aabb.size).intersects_segment(p_from, p_to)) {
					return true;
				}
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
		Ref<VoxelBlockyLibraryBase> library_ref = mesher_blocky->get_library();
		if (library_ref.is_null()) {
			return res;
		}
		RaycastPredicateBlocky predicate{ _terrain->get_storage(),
										  library_ref->get_baked_data(),
										  p_collision_mask,
										  local_pos,
										  local_pos + local_dir * max_distance };
		float hit_distance;
		float hit_distance_prev;
		if (zylann::voxel_raycast(
					local_pos, local_dir, predicate, max_distance, hit_pos, prev_pos, hit_distance, hit_distance_prev
			)) {
			res.instantiate();
			res->position = hit_pos;
			res->previous_position = prev_pos;
			res->distance_along_ray = hit_distance * to_world_scale;
		}

	} else if (try_get_as(_terrain->get_mesher(), mesher_cubes)) {
		RaycastPredicateColor predicate{ _terrain->get_storage() };
		float hit_distance;
		float hit_distance_prev;
		if (zylann::voxel_raycast(
					local_pos, local_dir, predicate, max_distance, hit_pos, prev_pos, hit_distance, hit_distance_prev
			)) {
			res.instantiate();
			res->position = hit_pos;
			res->previous_position = prev_pos;
			res->distance_along_ray = hit_distance * to_world_scale;
		}

	} else {
		RaycastPredicateSDF predicate{ _terrain->get_storage() };
		float hit_distance;
		float hit_distance_prev;
		if (zylann::voxel_raycast(
					local_pos, local_dir, predicate, max_distance, hit_pos, prev_pos, hit_distance, hit_distance_prev
			)) {
			res.instantiate();
			res->position = hit_pos;
			res->previous_position = prev_pos;
			res->distance_along_ray = hit_distance * to_world_scale;
		}
	}

	return res;
}

void VoxelToolTerrain::copy(Vector3i pos, VoxelBuffer &dst, uint8_t channels_mask) const {
	ERR_FAIL_COND(_terrain == nullptr);
	if (channels_mask == 0) {
		channels_mask = (1 << _channel);
	}
	_terrain->get_storage().copy(pos, dst, channels_mask);
}

void VoxelToolTerrain::paste(Vector3i pos, const VoxelBuffer &src, uint8_t channels_mask) {
	ERR_FAIL_COND(_terrain == nullptr);
	if (channels_mask == 0) {
		channels_mask = (1 << _channel);
	}
	_terrain->get_storage().paste(pos, src, channels_mask, false);
	_post_edit(Box3i(pos, src.get_size()));
}

void VoxelToolTerrain::paste_masked(
		Vector3i pos,
		Ref<godot::VoxelBuffer> p_voxels,
		uint8_t channels_mask,
		uint8_t mask_channel,
		uint64_t mask_value
) {
	ERR_FAIL_COND(_terrain == nullptr);
	ERR_FAIL_COND(p_voxels.is_null());
	if (channels_mask == 0) {
		channels_mask = (1 << _channel);
	}
	_terrain->get_storage().paste_masked(pos, p_voxels->get_buffer(), channels_mask, mask_channel, mask_value, false);
	_post_edit(Box3i(pos, p_voxels->get_buffer().get_size()));
}

void VoxelToolTerrain::paste_masked_writable_list( //
		Vector3i pos, //
		Ref<godot::VoxelBuffer> p_voxels, //
		uint8_t channels_mask, //
		uint8_t src_mask_channel, //
		uint64_t src_mask_value, //
		uint8_t dst_mask_channel, //
		PackedInt32Array dst_writable_list //
) {
	ERR_FAIL_COND(_terrain == nullptr);
	ERR_FAIL_COND(p_voxels.is_null());
	if (channels_mask == 0) {
		channels_mask = (1 << _channel);
	}
	_terrain->get_storage().paste_masked_writable_list( //
			pos, //
			p_voxels->get_buffer(), //
			channels_mask, //
			src_mask_channel, //
			src_mask_value, //
			dst_mask_channel, //
			to_span(dst_writable_list),
			false //
	);
	_post_edit(Box3i(pos, p_voxels->get_buffer().get_size()));
}

void VoxelToolTerrain::do_box(Vector3i begin, Vector3i end) {
	ZN_PROFILE_SCOPE();
	ERR_FAIL_COND(_terrain == nullptr);

	if (get_channel() != VoxelBuffer::CHANNEL_SDF) {
		// Fallback on generic do_box, which pretty much does a naive fill in the exact boundaries, though it's still
		// slower than necessary because it uses random access.
		// TODO Make it so generic ops can do that too without an extra margin and without superfluous calculations
		VoxelTool::do_box(begin, end);
		return;
	}

	ops::DoShapeChunked<ops::SdfAxisAlignedBox, ops::VoxelDataGridAccess> op;
	op.shape.center = to_vec3f(begin + end) * 0.5f;
	op.shape.half_size = to_vec3f(end - begin) * 0.5f;
	op.shape.sdf_scale = get_sdf_scale();
	op.box = op.shape.get_box().clipped(_terrain->get_bounds());
	op.mode = ops::Mode(get_mode());
	op.texture_params = _texture_params;
	op.blocky_value = _value;
	op.channel = get_channel();
	op.strength = get_sdf_strength();

	if (!is_area_editable(op.box)) {
		ZN_PRINT_WARNING("Area not editable");
		return;
	}

	VoxelData &data = _terrain->get_storage();

	VoxelDataGrid grid;
	data.get_blocks_grid(grid, op.box, 0);
	op.block_access.grid = &grid;

	{
		VoxelDataGrid::LockWrite wlock(grid);
		op();
	}

	_post_edit(op.box);
}

void VoxelToolTerrain::do_sphere(Vector3 center, float radius) {
	ZN_PROFILE_SCOPE();
	ERR_FAIL_COND(_terrain == nullptr);

	ops::DoSphere op;
	op.shape.center = to_vec3f(center);
	op.shape.radius = radius;
	op.shape.sdf_scale = get_sdf_scale();
	op.box = op.shape.get_box().clipped(_terrain->get_bounds());
	op.mode = ops::Mode(get_mode());
	op.texture_params = _texture_params;
	op.blocky_value = _value;
	op.channel = get_channel();
	op.strength = get_sdf_strength();

	if (!is_area_editable(op.box)) {
		ZN_PRINT_WARNING("Area not editable");
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

	ops::DoShapeChunked<ops::SdfHemisphere, ops::VoxelDataGridAccess> op;
	op.shape.center = to_vec3f(center);
	op.shape.radius = radius;
	op.shape.flat_direction = to_vec3f(flat_direction);
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
		ZN_PRINT_WARNING("Area not editable");
		return;
	}

	VoxelData &data = _terrain->get_storage();

	VoxelDataGrid grid;
	data.get_blocks_grid(grid, op.box, 0);
	op.block_access.grid = &grid;

	{
		VoxelDataGrid::LockWrite wlock(grid);
		op();
	}

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
	_terrain->post_edit_area(box, true);
}

void VoxelToolTerrain::set_voxel_metadata(Vector3i pos, Variant meta) {
	ERR_FAIL_COND(_terrain == nullptr);
	VoxelData &data = _terrain->get_storage();
	data.set_voxel_metadata(pos, meta);
	_terrain->post_edit_area(Box3i(pos, Vector3i(1, 1, 1)), false);
}

Variant VoxelToolTerrain::get_voxel_metadata(Vector3i pos) const {
	ERR_FAIL_COND_V(_terrain == nullptr, Variant());
	VoxelData &data = _terrain->get_storage();
	return data.get_voxel_metadata(pos);
}

namespace {
Ref<VoxelBlockyLibraryBase> get_voxel_library(const VoxelTerrain &terrain) {
	Ref<VoxelMesherBlocky> blocky_mesher = terrain.get_mesher();
	if (blocky_mesher.is_valid()) {
		return blocky_mesher->get_library();
	}
	return Ref<VoxelBlockyLibraryBase>();
}
} // namespace

// TODO This function snaps the given AABB to blocks, this is not intuitive. Should figure out a way to respect the
// area. Executes a function on random voxels in the provided area, using the type channel. This allows to implement
// slow "natural" cellular automata behavior, as can be seen in Minecraft.
void VoxelToolTerrain::run_blocky_random_tick(
		AABB voxel_area,
		int voxel_count,
		const Callable &callback,
		int batch_count
) {
	ZN_PROFILE_SCOPE();

	ERR_FAIL_COND(_terrain == nullptr);
	ERR_FAIL_COND_MSG(
			get_voxel_library(*_terrain).is_null(),
			String("This function requires a volume using {0} with a valid library")
					.format(varray(VoxelMesherBlocky::get_class_static()))
	);
	ERR_FAIL_COND(callback.is_null());
	ERR_FAIL_COND(batch_count <= 0);
	ERR_FAIL_COND(voxel_count < 0);
	ERR_FAIL_COND(!math::is_valid_size(voxel_area.size));

	if (voxel_count == 0) {
		return;
	}

	const VoxelBlockyLibraryBase &lib = **get_voxel_library(*_terrain);
	VoxelData &data = _terrain->get_storage();

	zylann::voxel::run_blocky_random_tick(data, voxel_area, lib, _random, voxel_count, batch_count, callback);
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
		std::shared_ptr<VoxelBuffer> voxels_ptr = data.try_get_block_voxels(block_pos);

		if (voxels_ptr == nullptr) {
			return;
		}

		const Vector3i block_origin = block_pos * data.get_block_size();
		const Box3i rel_voxel_box(voxel_box.position - block_origin, voxel_box.size);
		// TODO Worth it locking blocks for metadata?
		// For read or write? We'd have to specify as argument and trust the user... since metadata can contain
		// reference types.

		voxels_ptr->for_each_voxel_metadata_in_area(
				rel_voxel_box,
				[&callback, block_origin](Vector3i rel_pos, const VoxelMetadata &meta) {
					const Variant v = godot::get_as_variant(meta);
					const Vector3i key = rel_pos + block_origin;
#ifdef ZN_GODOT
					const Variant key_v = key;
					const Variant *args[2] = { &key_v, &v };
					Callable::CallError err;
					Variant retval; // We don't care about the return value, Callable API requires it
					callback.callp(args, 2, retval, err);

					ERR_FAIL_COND_MSG(
							err.error != Callable::CallError::CALL_OK,
							String("Callable failed at {0}").format(varray(key))
					);
#elif defined(ZN_GODOT_EXTENSION)
					// TODO GDX: No way to detect or report errors when calling a Callable. Do I need to?
					callback.call(key, v);
#endif
				}
		);
	});
}

void VoxelToolTerrain::do_path(Span<const Vector3> positions, Span<const float> radii) {
	ZN_PROFILE_SCOPE();
	ZN_ASSERT_RETURN(positions.size() >= 2);
	ZN_ASSERT_RETURN(positions.size() == radii.size());

	// TODO Increase margin a bit with smooth voxels?
	const int margin = 1;

	// Compute total bounding box

	const AABB total_aabb = get_path_aabb(positions, radii).grow(margin);
	const Box3i total_voxel_box(to_vec3i(math::floor(total_aabb.position)), to_vec3i(math::ceil(total_aabb.size)));

	VoxelDataGrid grid;

	VoxelData &data = _terrain->get_storage();

	data.get_blocks_grid(grid, total_voxel_box, 0);

	{
		VoxelDataGrid::LockWrite wlock(grid);

		// Rasterize

		for (unsigned int point_index = 1; point_index < positions.size(); ++point_index) {
			// TODO Could run this in local space so we dont need doubles
			// TODO Apply terrain scale
			const Vector3f p0 = to_vec3f(positions[point_index - 1]);
			const Vector3f p1 = to_vec3f(positions[point_index]);

			const float r0 = radii[point_index - 1];
			const float r1 = radii[point_index];

			ops::DoShapeChunked<ops::SdfRoundCone, ops::VoxelDataGridAccess> op;
			op.block_access.grid = &grid;
			op.shape.cone.a = p0;
			op.shape.cone.b = p1;
			op.shape.cone.r1 = r0;
			op.shape.cone.r2 = r1;
			op.shape.cone.update();
			op.shape.sdf_scale = get_sdf_scale();
			op.box = op.shape.get_box().padded(margin);
			op.mode = ops::Mode(get_mode());
			op.texture_params = _texture_params;
			op.blocky_value = _value;
			op.channel = get_channel();
			op.strength = get_sdf_strength();

			op();

			// Experimented with drawing a 100-point path, the cost of everything outside cone calculation was:
			// - Non-template: 2.55 ms
			// - Template: 1.00 ms
			// With cone calculation:
			// - Non-template: 5.7 ms
			// - Template: 4.5 ms
			// So the template version is faster, but not that much.
			//
			// math::SdfRoundConePrecalc cone;
			// cone.a = p0;
			// cone.b = p1;
			// cone.r1 = r0;
			// cone.r2 = r1;
			// cone.update();
			// uint64_t value = _value;
			// segment_box.for_each_cell_zxy([&grid, &cone, value](Vector3i pos) {
			// 	if (cone(pos) < 0.f) {
			// 		grid.set_voxel_no_lock(pos, value, VoxelBuffer::CHANNEL_TYPE);
			// 	}
			// });
		}
	}

	_post_edit(total_voxel_box);
}

void VoxelToolTerrain::_bind_methods() {
	ClassDB::bind_method(
			D_METHOD("run_blocky_random_tick", "area", "voxel_count", "callback", "batch_count"),
			&VoxelToolTerrain::run_blocky_random_tick,
			DEFVAL(16)
	);
	ClassDB::bind_method(
			D_METHOD("for_each_voxel_metadata_in_area", "voxel_area", "callback"),
			&VoxelToolTerrain::for_each_voxel_metadata_in_area
	);
	ClassDB::bind_method(
			D_METHOD("do_hemisphere", "center", "radius", "flat_direction", "smoothness"),
			&VoxelToolTerrain::do_hemisphere,
			DEFVAL(0.0)
	);
}

} // namespace zylann::voxel
