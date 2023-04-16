#include "voxel_tool_lod_terrain.h"
#include "../constants/voxel_string_names.h"
#include "../generators/graph/voxel_generator_graph.h"
#include "../storage/voxel_buffer_gd.h"
#include "../storage/voxel_data_grid.h"
#include "../terrain/variable_lod/voxel_lod_terrain.h"
#include "../util/dstack.h"
#include "../util/godot/classes/collision_shape_3d.h"
#include "../util/godot/classes/convex_polygon_shape_3d.h"
#include "../util/godot/classes/mesh.h"
#include "../util/godot/classes/mesh_instance_3d.h"
#include "../util/godot/classes/rigid_body_3d.h"
#include "../util/godot/classes/timer.h"
#include "../util/godot/core/callable.h"
#include "../util/island_finder.h"
#include "../util/math/conv.h"
#include "../util/tasks/async_dependency_tracker.h"
#include "../util/voxel_raycast.h"
#include "funcs.h"
#include "voxel_mesh_sdf_gd.h"

namespace zylann::voxel {

VoxelToolLodTerrain::VoxelToolLodTerrain(VoxelLodTerrain *terrain) : _terrain(terrain) {
	ERR_FAIL_COND(terrain == nullptr);
	// At the moment, only LOD0 is supported.
	// Don't destroy the terrain while a voxel tool still references it
}

bool VoxelToolLodTerrain::is_area_editable(const Box3i &box) const {
	ERR_FAIL_COND_V(_terrain == nullptr, false);
	return _terrain->get_storage().is_area_loaded(box);
}

// Binary search can be more accurate than linear regression because the SDF can be inaccurate in the first place.
// An alternative would be to polygonize a tiny area around the middle-phase hit position.
// `d1` is how far from `pos0` along `dir` the binary search will take place.
// The segment may be adjusted internally if it does not contain a zero-crossing of the
template <typename Volume_F>
float approximate_distance_to_isosurface_binary_search(
		const Volume_F &f, Vector3 pos0, Vector3 dir, float d1, int iterations) {
	float d0 = 0.f;
	float sdf0 = get_sdf_interpolated(f, pos0);
	// The position given as argument may be a rough approximation coming from the middle-phase,
	// so it can be slightly below the surface. We can adjust it a little so it is above.
	for (int i = 0; i < 4 && sdf0 < 0.f; ++i) {
		d0 -= 0.5f;
		sdf0 = get_sdf_interpolated(f, pos0 + dir * d0);
	}

	float sdf1 = get_sdf_interpolated(f, pos0 + dir * d1);
	for (int i = 0; i < 4 && sdf1 > 0.f; ++i) {
		d1 += 0.5f;
		sdf1 = get_sdf_interpolated(f, pos0 + dir * d1);
	}

	if ((sdf0 > 0) != (sdf1 > 0)) {
		// Binary search
		for (int i = 0; i < iterations; ++i) {
			const float dm = 0.5f * (d0 + d1);
			const float sdf_mid = get_sdf_interpolated(f, pos0 + dir * dm);

			if ((sdf_mid > 0) != (sdf0 > 0)) {
				sdf1 = sdf_mid;
				d1 = dm;
			} else {
				sdf0 = sdf_mid;
				d0 = dm;
			}
		}
	}

	// Pick distance closest to the surface
	if (Math::abs(sdf0) < Math::abs(sdf1)) {
		return d0;
	} else {
		return d1;
	}
}

Ref<VoxelRaycastResult> VoxelToolLodTerrain::raycast(
		Vector3 pos, Vector3 dir, float max_distance, uint32_t collision_mask) {
	// TODO Transform input if the terrain is rotated
	// TODO Implement reverse raycast? (going from inside ground to air, could be useful for undigging)

	// TODO Optimization: voxel raycast uses `get_voxel` which is the slowest, but could be made faster.
	// Instead, do a broad-phase on blocks. If a block's voxels need to be parsed, get all positions the ray could go
	// through in that block, then query them all at once (better for bulk processing without going again through
	// locking and data structures, and allows SIMD). Then check results in order.
	// If no hit is found, carry on with next blocks.

	struct RaycastPredicate {
		VoxelData &data;

		bool operator()(const VoxelRaycastState &rs) {
			// This is not particularly optimized, but runs fast enough for player raycasts
			VoxelSingleValue defval;
			defval.f = 1.f;
			const VoxelSingleValue v = data.get_voxel(rs.hit_position, VoxelBufferInternal::CHANNEL_SDF, defval);
			return v.f < 0;
		}
	};

	Ref<VoxelRaycastResult> res;

	// We use grid-raycast as a middle-phase to roughly detect where the hit will be
	RaycastPredicate predicate = { _terrain->get_storage() };
	Vector3i hit_pos;
	Vector3i prev_pos;
	float hit_distance;
	float hit_distance_prev;
	// Voxels polygonized using marching cubes influence a region centered on their lower corner,
	// and extend up to 0.5 units in all directions.
	//
	//   o--------o--------o
	//   | A      |     B  |  Here voxel B is full, voxels A, C and D are empty.
	//   |       xxx       |  Matter will show up at the lower corner of B due to interpolation.
	//   |     xxxxxxx     |
	//   o---xxxxxoxxxxx---o
	//   |     xxxxxxx     |
	//   |       xxx       |
	//   | C      |     D  |
	//   o--------o--------o
	//
	// `voxel_raycast` operates on a discrete grid of cubic voxels, so to account for the smooth interpolation,
	// we may offset the ray so that cubes act as if they were centered on the filtered result.
	const Vector3 offset(0.5, 0.5, 0.5);
	if (voxel_raycast(pos + offset, dir, predicate, max_distance, hit_pos, prev_pos, hit_distance, hit_distance_prev)) {
		// Approximate surface

		float d = hit_distance;

		if (_raycast_binary_search_iterations > 0) {
			// This is not particularly optimized, but runs fast enough for player raycasts
			struct VolumeSampler {
				VoxelData &data;

				inline float operator()(const Vector3i &pos) const {
					VoxelSingleValue defval;
					defval.f = 1.f;
					const VoxelSingleValue value = data.get_voxel(pos, VoxelBufferInternal::CHANNEL_SDF, defval);
					return value.f;
				}
			};

			VolumeSampler sampler{ _terrain->get_storage() };
			d = hit_distance_prev +
					approximate_distance_to_isosurface_binary_search(sampler, pos + dir * hit_distance_prev, dir,
							hit_distance - hit_distance_prev, _raycast_binary_search_iterations);
		}

		res.instantiate();
		res->position = hit_pos;
		res->previous_position = prev_pos;
		res->distance_along_ray = d;
	}

	return res;
}

void VoxelToolLodTerrain::do_sphere(Vector3 center, float radius) {
	ZN_PROFILE_SCOPE();
	ERR_FAIL_COND(_terrain == nullptr);

	ops::DoSphere op;
	op.shape.center = center;
	op.shape.radius = radius;
	op.shape.sdf_scale = get_sdf_scale();
	op.box = op.shape.get_box().clipped(_terrain->get_voxel_bounds());
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

	data.pre_generate_box(op.box);
	data.get_blocks_grid(op.blocks, op.box, 0);
	op();

	_post_edit(op.box);
}

void VoxelToolLodTerrain::do_hemisphere(Vector3 center, float radius, Vector3 flat_direction, float smoothness) {
	ZN_PROFILE_SCOPE();
	ERR_FAIL_COND(_terrain == nullptr);

	ops::DoHemisphere op;
	op.shape.center = center;
	op.shape.radius = radius;
	op.shape.flat_direction = flat_direction;
	op.shape.plane_d = flat_direction.dot(center);
	op.shape.smoothness = smoothness;
	op.shape.sdf_scale = get_sdf_scale();
	op.box = op.shape.get_box().clipped(_terrain->get_voxel_bounds());
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

	data.pre_generate_box(op.box);
	data.get_blocks_grid(op.blocks, op.box, 0);
	op();

	_post_edit(op.box);
}

template <typename Op_T>
class VoxelToolAsyncEdit : public IThreadedTask {
public:
	VoxelToolAsyncEdit(Op_T op, std::shared_ptr<VoxelData> data) : _op(op), _data(data) {
		_tracker = make_shared_instance<AsyncDependencyTracker>(1);
	}

	const char *get_debug_name() const override {
		return "VoxelToolAsyncEdit";
	}

	void run(ThreadedTaskContext ctx) override {
		ZN_PROFILE_SCOPE();
		ZN_ASSERT(_data != nullptr);
		// TODO Thread-safety: not sure if this is entirely safe, VoxelDataBlock members aren't protected.
		// Only the map and VoxelBuffers are. To fix this we could migrate to a spatial lock.

		// TODO May want to fail if not all blocks were found
		// TODO Need to apply modifiers
		_data->get_blocks_grid(_op.blocks, _op.box, 0);
		_op();
		_tracker->post_complete();
	}

	std::shared_ptr<AsyncDependencyTracker> get_tracker() {
		return _tracker;
	}

private:
	Op_T _op;
	// We reference this just to keep map pointers alive
	std::shared_ptr<VoxelData> _data;
	std::shared_ptr<AsyncDependencyTracker> _tracker;
};

void VoxelToolLodTerrain::do_sphere_async(Vector3 center, float radius) {
	ERR_FAIL_COND(_terrain == nullptr);

	ops::DoSphere op;
	op.shape.center = center;
	op.shape.radius = radius;
	op.shape.sdf_scale = get_sdf_scale();
	op.box = op.shape.get_box().clipped(_terrain->get_voxel_bounds());
	op.mode = ops::Mode(get_mode());
	op.texture_params = _texture_params;
	op.blocky_value = _value;
	op.channel = get_channel();
	op.strength = get_sdf_strength();

	if (!is_area_editable(op.box)) {
		ZN_PRINT_VERBOSE("Area not editable");
		return;
	}

	std::shared_ptr<VoxelData> data = _terrain->get_storage_shared();

	VoxelToolAsyncEdit<ops::DoSphere> *task = memnew(VoxelToolAsyncEdit<ops::DoSphere>(op, data));
	_terrain->push_async_edit(task, op.box, task->get_tracker());
}

void VoxelToolLodTerrain::copy(Vector3i pos, Ref<gd::VoxelBuffer> dst, uint8_t channels_mask) const {
	ERR_FAIL_COND(_terrain == nullptr);
	ERR_FAIL_COND(dst.is_null());
	if (channels_mask == 0) {
		channels_mask = (1 << _channel);
	}
	_terrain->get_storage().copy(pos, dst->get_buffer(), channels_mask);
}

void VoxelToolLodTerrain::paste(Vector3i pos, Ref<gd::VoxelBuffer> dst, uint8_t channels_mask) {
	ERR_FAIL_COND(_terrain == nullptr);
	ERR_FAIL_COND(dst.is_null());
	if (channels_mask == 0) {
		channels_mask = (1 << _channel);
	}
	const Box3i box(pos, dst->get_size());
	if (!is_area_editable(box)) {
		ZN_PRINT_VERBOSE("Area not editable");
		return;
	}

	VoxelData &data = _terrain->get_storage();

	data.pre_generate_box(box);
	data.paste(pos, dst->get_buffer(), channels_mask, false);

	_post_edit(box);
}

float VoxelToolLodTerrain::get_voxel_f_interpolated(Vector3 position) const {
	ZN_PROFILE_SCOPE();
	ERR_FAIL_COND_V(_terrain == nullptr, 0);
	const int channel = get_channel();
	VoxelData &data = _terrain->get_storage();
	// TODO Optimization: is it worth a making a fast-path for this?
	return get_sdf_interpolated(
			[&data, channel](Vector3i ipos) {
				VoxelSingleValue defval;
				defval.f = 1.f;
				VoxelSingleValue value = data.get_voxel(ipos, channel, defval);
				return value.f;
			},
			position);
}

uint64_t VoxelToolLodTerrain::_get_voxel(Vector3i pos) const {
	ERR_FAIL_COND_V(_terrain == nullptr, 0);
	VoxelSingleValue defval;
	defval.i = 0;
	return _terrain->get_storage().get_voxel(pos, _channel, defval).i;
}

float VoxelToolLodTerrain::_get_voxel_f(Vector3i pos) const {
	ERR_FAIL_COND_V(_terrain == nullptr, 0);
	VoxelSingleValue defval;
	defval.f = 1.f;
	return _terrain->get_storage().get_voxel(pos, _channel, defval).f;
}

void VoxelToolLodTerrain::_set_voxel(Vector3i pos, uint64_t v) {
	ERR_FAIL_COND(_terrain == nullptr);
	_terrain->get_storage().try_set_voxel(v, pos, _channel);
	// No post_update, the parent class does it, it's a generic slow implemntation
}

void VoxelToolLodTerrain::_set_voxel_f(Vector3i pos, float v) {
	ERR_FAIL_COND(_terrain == nullptr);
	// TODO Format should be accessible from terrain
	_terrain->get_storage().try_set_voxel_f(v, pos, _channel);
	// No post_update, the parent class does it, it's a generic slow implemntation
}

void VoxelToolLodTerrain::_post_edit(const Box3i &box) {
	ERR_FAIL_COND(_terrain == nullptr);
	_terrain->post_edit_area(box);
}

int VoxelToolLodTerrain::get_raycast_binary_search_iterations() const {
	return _raycast_binary_search_iterations;
}

void VoxelToolLodTerrain::set_raycast_binary_search_iterations(int iterations) {
	_raycast_binary_search_iterations = math::clamp(iterations, 0, 16);
}

// Turns floating chunks of voxels into rigidbodies:
// Detects separate groups of connected voxels within a box. Each group fully contained in the box is removed from
// the source volume, and turned into a rigidbody.
// This is one way of doing it, I don't know if it's the best way (there is rarely a best way)
// so there are probably other approaches that could be explored in the future, if they have better performance
Array separate_floating_chunks(VoxelTool &voxel_tool, Box3i world_box, Node *parent_node, Transform3D transform,
		Ref<VoxelMesher> mesher, Array materials) {
	ZN_PROFILE_SCOPE();

	// Checks
	ERR_FAIL_COND_V(mesher.is_null(), Array());
	ERR_FAIL_COND_V(parent_node == nullptr, Array());

	// Copy source data

	// TODO Do not assume channel, at the moment it's hardcoded for smooth terrain
	static const int channels_mask = (1 << VoxelBufferInternal::CHANNEL_SDF);
	static const VoxelBufferInternal::ChannelId main_channel = VoxelBufferInternal::CHANNEL_SDF;

	// TODO We should be able to use `VoxelBufferInternal`, just needs some things exposed
	Ref<gd::VoxelBuffer> source_copy_buffer_ref;
	{
		ZN_PROFILE_SCOPE_NAMED("Copy");
		source_copy_buffer_ref.instantiate();
		source_copy_buffer_ref->create(world_box.size.x, world_box.size.y, world_box.size.z);
		voxel_tool.copy(world_box.pos, source_copy_buffer_ref, channels_mask);
	}
	VoxelBufferInternal &source_copy_buffer = source_copy_buffer_ref->get_buffer();

	// Label distinct voxel groups

	static thread_local std::vector<uint8_t> ccl_output;
	ccl_output.resize(Vector3iUtil::get_volume(world_box.size));

	unsigned int label_count = 0;

	{
		// TODO Allow to run the algorithm at a different LOD, to trade precision for speed
		ZN_PROFILE_SCOPE_NAMED("CCL scan");
		IslandFinder island_finder;
		island_finder.scan_3d(
				Box3i(Vector3i(), world_box.size),
				[&source_copy_buffer](Vector3i pos) {
					// TODO Can be optimized further with direct access
					return source_copy_buffer.get_voxel_f(pos.x, pos.y, pos.z, main_channel) < 0.f;
				},
				to_span(ccl_output), &label_count);
	}

	struct Bounds {
		Vector3i min_pos;
		Vector3i max_pos; // inclusive
		bool valid = false;
	};

	// Compute bounds of each group

	std::vector<Bounds> bounds_per_label;
	{
		ZN_PROFILE_SCOPE_NAMED("Bounds calculation");

		// Adding 1 because label 0 is the index for "no label"
		bounds_per_label.resize(label_count + 1);

		unsigned int ccl_index = 0;
		for (int z = 0; z < world_box.size.z; ++z) {
			for (int x = 0; x < world_box.size.x; ++x) {
				for (int y = 0; y < world_box.size.y; ++y) {
					CRASH_COND(ccl_index >= ccl_output.size());
					const uint8_t label = ccl_output[ccl_index];
					++ccl_index;

					if (label == 0) {
						continue;
					}

					CRASH_COND(label >= bounds_per_label.size());
					Bounds &bounds = bounds_per_label[label];

					if (bounds.valid == false) {
						bounds.min_pos = Vector3i(x, y, z);
						bounds.max_pos = bounds.min_pos;
						bounds.valid = true;

					} else {
						if (x < bounds.min_pos.x) {
							bounds.min_pos.x = x;
						} else if (x > bounds.max_pos.x) {
							bounds.max_pos.x = x;
						}

						if (y < bounds.min_pos.y) {
							bounds.min_pos.y = y;
						} else if (y > bounds.max_pos.y) {
							bounds.max_pos.y = y;
						}

						if (z < bounds.min_pos.z) {
							bounds.min_pos.z = z;
						} else if (z > bounds.max_pos.z) {
							bounds.max_pos.z = z;
						}
					}
				}
			}
		}
	}

	// Eliminate groups that touch the box border,
	// because that means we can't tell if they are truly hanging in the air or attached to land further away

	const Vector3i lbmax = world_box.size - Vector3i(1, 1, 1);
	for (unsigned int label = 1; label < bounds_per_label.size(); ++label) {
		CRASH_COND(label >= bounds_per_label.size());
		Bounds &local_bounds = bounds_per_label[label];
		ERR_CONTINUE(!local_bounds.valid);

		if (local_bounds.min_pos.x == 0 || local_bounds.min_pos.y == 0 || local_bounds.min_pos.z == 0 ||
				local_bounds.max_pos.x == lbmax.x || local_bounds.max_pos.y == lbmax.y ||
				local_bounds.max_pos.z == lbmax.z) {
			//
			local_bounds.valid = false;
		}
	}

	// Create voxel buffer for each group

	struct InstanceInfo {
		Ref<gd::VoxelBuffer> voxels;
		Vector3i world_pos;
		unsigned int label;
	};
	std::vector<InstanceInfo> instances_info;

	const int min_padding = 2; // mesher->get_minimum_padding();
	const int max_padding = 2; // mesher->get_maximum_padding();

	{
		ZN_PROFILE_SCOPE_NAMED("Extraction");

		for (unsigned int label = 1; label < bounds_per_label.size(); ++label) {
			CRASH_COND(label >= bounds_per_label.size());
			const Bounds local_bounds = bounds_per_label[label];

			if (!local_bounds.valid) {
				continue;
			}

			const Vector3i world_pos = world_box.pos + local_bounds.min_pos - Vector3iUtil::create(min_padding);
			const Vector3i size =
					local_bounds.max_pos - local_bounds.min_pos + Vector3iUtil::create(1 + max_padding + min_padding);

			// TODO We should be able to use `VoxelBufferInternal`, just needs some things exposed
			Ref<gd::VoxelBuffer> buffer_ref;
			buffer_ref.instantiate();
			buffer_ref->create(size.x, size.y, size.z);

			// Read voxels from the source volume
			voxel_tool.copy(world_pos, buffer_ref, channels_mask);

			VoxelBufferInternal &buffer = buffer_ref->get_buffer();

			// Cleanup padding borders
			const Box3i inner_box(Vector3iUtil::create(min_padding),
					buffer.get_size() - Vector3iUtil::create(min_padding + max_padding));
			Box3i(Vector3i(), buffer.get_size()).difference(inner_box, [&buffer](Box3i box) {
				buffer.fill_area_f(1.f, box.pos, box.pos + box.size, main_channel);
			});

			// Filter out voxels that don't belong to this label
			for (int z = local_bounds.min_pos.z; z <= local_bounds.max_pos.z; ++z) {
				for (int x = local_bounds.min_pos.x; x <= local_bounds.max_pos.x; ++x) {
					for (int y = local_bounds.min_pos.y; y <= local_bounds.max_pos.y; ++y) {
						const unsigned int ccl_index = Vector3iUtil::get_zxy_index(Vector3i(x, y, z), world_box.size);
						CRASH_COND(ccl_index >= ccl_output.size());
						const uint8_t label2 = ccl_output[ccl_index];

						if (label2 != 0 && label != label2) {
							buffer.set_voxel_f(1.f, min_padding + x - local_bounds.min_pos.x,
									min_padding + y - local_bounds.min_pos.y, min_padding + z - local_bounds.min_pos.z,
									main_channel);
						}
					}
				}
			}

			instances_info.push_back(InstanceInfo{ buffer_ref, world_pos, label });
		}
	}

	// Erase voxels from source volume.
	// Must be done after we copied voxels from it.

	{
		ZN_PROFILE_SCOPE_NAMED("Erasing");

		voxel_tool.set_channel(main_channel);

		for (unsigned int instance_index = 0; instance_index < instances_info.size(); ++instance_index) {
			CRASH_COND(instance_index >= instances_info.size());
			const InstanceInfo info = instances_info[instance_index];
			ERR_CONTINUE(info.voxels.is_null());

			voxel_tool.sdf_stamp_erase(info.voxels, info.world_pos);
		}
	}

	// Find out which materials contain parameters that require instancing.
	//
	// Since 7dbc458bb4f3e0cc94e5070bd33bde41d214c98d it's no longer possible to quickly check if a
	// shader has a uniform by name using Shader's parameter cache. Now it seems the only way is to get the whole list
	// of parameters and find into it, which is slow, tedious to write and different between modules and GDExtension.

	uint32_t materials_to_instance_mask = 0;
	{
		std::vector<GodotShaderParameterInfo> params;
		const String u_block_local_transform = VoxelStringNames::get_singleton().u_block_local_transform;

		ZN_ASSERT_RETURN_V_MSG(materials.size() < 32, Array(),
				"Too many materials. If you need more, make a request or change the code.");

		for (int material_index = 0; material_index < materials.size(); ++material_index) {
			Ref<ShaderMaterial> sm = materials[material_index];
			if (sm.is_null()) {
				continue;
			}

			Ref<Shader> shader = sm->get_shader();
			if (shader.is_null()) {
				continue;
			}

			params.clear();
			get_shader_parameter_list(shader->get_rid(), params);

			for (const GodotShaderParameterInfo &param_info : params) {
				if (param_info.name == u_block_local_transform) {
					materials_to_instance_mask |= (1 << material_index);
					break;
				}
			}
		}
	}

	// Create instances

	Array nodes;

	{
		ZN_PROFILE_SCOPE_NAMED("Remeshing and instancing");

		for (unsigned int instance_index = 0; instance_index < instances_info.size(); ++instance_index) {
			CRASH_COND(instance_index >= instances_info.size());
			const InstanceInfo info = instances_info[instance_index];
			ERR_CONTINUE(info.voxels.is_null());

			CRASH_COND(info.label >= bounds_per_label.size());
			const Bounds local_bounds = bounds_per_label[info.label];
			ERR_CONTINUE(!local_bounds.valid);

			// DEBUG
			// print_line(String("--- Instance {0}").format(varray(instance_index)));
			// for (int z = 0; z < info.voxels->get_size().z; ++z) {
			// 	for (int x = 0; x < info.voxels->get_size().x; ++x) {
			// 		String s;
			// 		for (int y = 0; y < info.voxels->get_size().y; ++y) {
			// 			float sdf = info.voxels->get_voxel_f(x, y, z, VoxelBuffer::CHANNEL_SDF);
			// 			if (sdf < -0.1f) {
			// 				s += "X ";
			// 			} else if (sdf < 0.f) {
			// 				s += "x ";
			// 			} else {
			// 				s += "- ";
			// 			}
			// 		}
			// 		print_line(s);
			// 	}
			// 	print_line("//");
			// }

			const Transform3D local_transform(Basis(), info.world_pos);

			for (int i = 0; i < materials.size(); ++i) {
				if ((materials_to_instance_mask & (1 << i)) != 0) {
					Ref<ShaderMaterial> sm = materials[i];
					ZN_ASSERT_CONTINUE(sm.is_valid());
					sm = sm->duplicate(false);
					// That parameter should have a valid default value matching the local transform relative to the
					// volume, which is usually per-instance, but in Godot 3 we have no such feature, so we have to
					// duplicate.
					// TODO Try using per-instance parameters for scalar uniforms (Godot 4 doesn't support textures)
					sm->set_shader_parameter(
							VoxelStringNames::get_singleton().u_block_local_transform, local_transform);
					materials[i] = sm;
				}
			}

			// TODO If normalmapping is used here with the Transvoxel mesher, we need to either turn it off just for
			// this call, or to pass the right options
			Ref<Mesh> mesh = mesher->build_mesh(info.voxels, materials, Dictionary());
			// The mesh is not supposed to be null,
			// because we build these buffers from connected groups that had negative SDF.
			ERR_CONTINUE(mesh.is_null());

			if (is_mesh_empty(**mesh)) {
				continue;
			}

			// DEBUG
			// {
			// 	Ref<VoxelBlockSerializer> serializer;
			// 	serializer.instance();
			// 	Ref<StreamPeerBuffer> peer;
			// 	peer.instance();
			// 	serializer->serialize(peer, info.voxels, false);
			// 	String fpath = String("debug_data/split_dump_{0}.bin").format(varray(instance_index));
			// 	FileAccess *f = FileAccess::open(fpath, FileAccess::WRITE);
			// 	PoolByteArray bytes = peer->get_data_array();
			// 	PoolByteArray::Read bytes_read = bytes.read();
			// 	f->store_buffer(bytes_read.ptr(), bytes.size());
			// 	f->close();
			// 	memdelete(f);
			// }

			// TODO Option to make multiple convex shapes
			// TODO Use the fast way. This is slow because of the internal TriangleMesh thing.
			// TODO Don't create a body if the mesh has no triangles
			Ref<Shape3D> shape = mesh->create_convex_shape();
			ERR_CONTINUE(shape.is_null());
			CollisionShape3D *collision_shape = memnew(CollisionShape3D);
			collision_shape->set_shape(shape);
			// Center the shape somewhat, because Godot is confusing node origin with center of mass
			const Vector3i size =
					local_bounds.max_pos - local_bounds.min_pos + Vector3iUtil::create(1 + max_padding + min_padding);
			const Vector3 offset = -Vector3(size) * 0.5f;
			collision_shape->set_position(offset);

			RigidBody3D *rigid_body = memnew(RigidBody3D);
			rigid_body->set_transform(transform * transform3d_translated_local(local_transform, -offset));
			rigid_body->add_child(collision_shape);
			rigid_body->set_freeze_mode(RigidBody3D::FREEZE_MODE_KINEMATIC);
			rigid_body->set_freeze_enabled(true);

			// Switch to rigid after a short time to workaround clipping with terrain,
			// because colliders are updated asynchronously
			Timer *timer = memnew(Timer);
			timer->set_wait_time(0.2);
			timer->set_one_shot(true);
#if defined(ZN_GODOT)
			timer->connect("timeout", ZN_GODOT_CALLABLE_MP(rigid_body, RigidBody3D, set_freeze_enabled).bind(false));
#elif defined(ZN_GODOT_EXTENSION)
			// TODO GDX: Callable::bind() cannot be used
			ZN_PRINT_ERROR("Callable::bind() cannot be used in GDExtension, can't apply clipping fix to RigidBody3D");
#endif
			// Cannot use start() here because it requires to be inside the SceneTree,
			// and we don't know if it will be after we add to the parent.
			timer->set_autostart(true);
			rigid_body->add_child(timer);

			MeshInstance3D *mesh_instance = memnew(MeshInstance3D);
			mesh_instance->set_mesh(mesh);
			mesh_instance->set_position(offset);
			rigid_body->add_child(mesh_instance);

			parent_node->add_child(rigid_body);

			nodes.append(rigid_body);
		}
	}

	return nodes;
}

#if defined(ZN_GODOT)
Array VoxelToolLodTerrain::separate_floating_chunks(AABB world_box, Node *parent_node) {
#elif defined(ZN_GODOT_EXTENSION)
Array VoxelToolLodTerrain::separate_floating_chunks(AABB world_box, Object *parent_node_o) {
	Node *parent_node = Object::cast_to<Node>(parent_node_o);
#endif
	ERR_FAIL_COND_V(_terrain == nullptr, Array());
	ERR_FAIL_COND_V(!math::is_valid_size(world_box.size), Array());
	Ref<VoxelMesher> mesher = _terrain->get_mesher();
	Array materials;
	materials.append(_terrain->get_material());
	const Box3i int_world_box(math::floor_to_int(world_box.position), math::ceil_to_int(world_box.size));
	return zylann::voxel::separate_floating_chunks(
			*this, int_world_box, parent_node, _terrain->get_global_transform(), mesher, materials);
}

// Combines a precalculated SDF with the terrain at a specific position, rotation and scale.
//
// `transform` is where the buffer should be applied on the terrain.
//
// `isolevel` alters the shape of the SDF: positive "puffs" it, negative "erodes" it. This is a applied after
// `sdf_scale`.
//
// `sdf_scale` scales SDF values (it doesnt make the shape bigger or smaller). Usually defaults to 1 but may be lower if
// artifacts show up due to scaling used in terrain SDF.
//
void VoxelToolLodTerrain::stamp_sdf(
		Ref<VoxelMeshSDF> mesh_sdf, Transform3D transform, float isolevel, float sdf_scale) {
	// TODO Asynchronous version
	ZN_PROFILE_SCOPE();

	ERR_FAIL_COND(_terrain == nullptr);
	ERR_FAIL_COND(mesh_sdf.is_null());
	ERR_FAIL_COND(mesh_sdf->is_baked());
	Ref<gd::VoxelBuffer> buffer_ref = mesh_sdf->get_voxel_buffer();
	ERR_FAIL_COND(buffer_ref.is_null());
	const VoxelBufferInternal &buffer = buffer_ref->get_buffer();
	const VoxelBufferInternal::ChannelId channel = VoxelBufferInternal::CHANNEL_SDF;
	ERR_FAIL_COND(buffer.get_channel_compression(channel) == VoxelBufferInternal::COMPRESSION_UNIFORM);
	ERR_FAIL_COND(buffer.get_channel_depth(channel) != VoxelBufferInternal::DEPTH_32_BIT);

	const Transform3D &box_to_world = transform;
	const AABB local_aabb = mesh_sdf->get_aabb();

	// Note, transform is local to the terrain
	const AABB aabb = box_to_world.xform(local_aabb);
	const Box3i voxel_box = Box3i::from_min_max(aabb.position.floor(), (aabb.position + aabb.size).ceil());

	// TODO Sometimes it will fail near unloaded blocks, even though the transformed box does not intersect them.
	// This could be avoided with a box/transformed-box intersection algorithm. Might investigate if the use case
	// occurs. It won't happen with full load mode. This also affects other shapes.
	if (!is_area_editable(voxel_box)) {
		ZN_PRINT_VERBOSE("Area not editable");
		return;
	}

	VoxelData &data = _terrain->get_storage();

	data.pre_generate_box(voxel_box);

	// TODO Maybe more efficient to "rasterize" the box? We're going to iterate voxels the box doesnt intersect
	// TODO Maybe we should scale SDF values based on the scale of the transform too

	const Transform3D buffer_to_box =
			Transform3D(Basis().scaled(Vector3(local_aabb.size / buffer.get_size())), local_aabb.position);
	const Transform3D buffer_to_world = box_to_world * buffer_to_box;

	// TODO Support other depths, format should be accessible from the volume
	ops::SdfOperation16bit<ops::SdfUnion, ops::SdfBufferShape> op;
	op.shape.world_to_buffer = buffer_to_world.affine_inverse();
	op.shape.buffer_size = buffer.get_size();
	op.shape.isolevel = isolevel;
	op.shape.sdf_scale = sdf_scale;
	// Note, the passed buffer must not be shared with another thread.
	// buffer.decompress_channel(channel);
	ZN_ASSERT_RETURN(buffer.get_channel_data(channel, op.shape.buffer));

	VoxelDataGrid grid;
	data.get_blocks_grid(grid, voxel_box, 0);
	grid.write_box(voxel_box, VoxelBufferInternal::CHANNEL_SDF, op);

	_post_edit(voxel_box);
}

// Runs the given graph in a bounding box in the terrain.
// The graph must have an SDF output and can also have an SDF input to read source voxels.
// The transform contains the position of the edit, its orientation and scale.
// Graph base size is the original size of the brush, as designed in the graph. It will be scaled using the transform.
void VoxelToolLodTerrain::do_graph(Ref<VoxelGeneratorGraph> graph, Transform3D transform, Vector3 graph_base_size) {
	ZN_PROFILE_SCOPE();
	ZN_DSTACK();
	ERR_FAIL_COND(_terrain == nullptr);

	const Vector3 area_size = math::abs(transform.basis.xform(graph_base_size));

	const Box3i box = Box3i::from_min_max( //
			math::floor_to_int(transform.origin - 0.5 * area_size),
			math::ceil_to_int(transform.origin + 0.5 * area_size))
							  .padded(2)
							  .clipped(_terrain->get_voxel_bounds());

	if (!is_area_editable(box)) {
		ZN_PRINT_VERBOSE("Area not editable");
		return;
	}

	VoxelData &data = _terrain->get_storage();

	data.pre_generate_box(box);

	const unsigned int channel_index = VoxelBufferInternal::CHANNEL_SDF;

	VoxelBufferInternal buffer;
	buffer.create(box.size);
	data.copy(box.pos, buffer, 1 << channel_index);

	buffer.decompress_channel(channel_index);

	// Convert input SDF
	static thread_local std::vector<float> tls_in_sdf_full;
	tls_in_sdf_full.resize(Vector3iUtil::get_volume(buffer.get_size()));
	Span<float> in_sdf_full = to_span(tls_in_sdf_full);
	get_unscaled_sdf(buffer, in_sdf_full);

	static thread_local std::vector<float> tls_in_x;
	static thread_local std::vector<float> tls_in_y;
	static thread_local std::vector<float> tls_in_z;
	const unsigned int deck_area = box.size.x * box.size.y;
	tls_in_x.resize(deck_area);
	tls_in_y.resize(deck_area);
	tls_in_z.resize(deck_area);
	Span<float> in_x = to_span(tls_in_x);
	Span<float> in_y = to_span(tls_in_y);
	Span<float> in_z = to_span(tls_in_z);

	const Transform3D inv_transform = transform.affine_inverse();

	const int output_sdf_buffer_index = graph->get_sdf_output_port_address();
	ZN_ASSERT_RETURN_MSG(output_sdf_buffer_index != -1, "The graph has no SDF output, cannot use it as a brush");

	// The graph works at a fixed dimension, so if we scale the operation with the Transform3D then we have to also
	// scale the distance field the graph is working at
	const float graph_scale = transform.basis.get_scale().length();
	const float inv_graph_scale = 1.f / graph_scale;

	for (unsigned int i = 0; i < in_sdf_full.size(); ++i) {
		in_sdf_full[i] *= inv_graph_scale;
	}

	const float op_strength = get_sdf_strength();

	{
		ZN_PROFILE_SCOPE_NAMED("Slices");
		// For each deck of the box (doing this to reduce memory usage since the graph will allocate temporary buffers
		// for each operation, which can be a lot depending on the complexity of the graph)
		Vector3i pos;
		const Vector3i endpos = box.pos + box.size;
		for (pos.z = box.pos.z; pos.z < endpos.z; ++pos.z) {
			// Set positions
			for (unsigned int i = 0; i < deck_area; ++i) {
				in_z[i] = pos.z;
			}
			{
				unsigned int i = 0;
				for (pos.x = box.pos.x; pos.x < endpos.x; ++pos.x) {
					for (pos.y = box.pos.y; pos.y < endpos.y; ++pos.y) {
						in_x[i] = pos.x;
						in_y[i] = pos.y;
						++i;
					}
				}
			}

			// Transform positions to be local to the graph
			for (unsigned int i = 0; i < deck_area; ++i) {
				Vector3 graph_local_pos(in_x[i], in_y[i], in_z[i]);
				graph_local_pos = inv_transform.xform(pos);
				in_x[i] = graph_local_pos.x;
				in_y[i] = graph_local_pos.y;
				in_z[i] = graph_local_pos.z;
			}

			// Get SDF input
			Span<float> in_sdf = in_sdf_full.sub(deck_area * (pos.z - box.pos.z), deck_area);

			// Run graph
			graph->generate_series(in_x, in_y, in_z, in_sdf);

			// Read result
			const pg::Runtime::State &state = VoxelGeneratorGraph::get_last_state_from_current_thread();
			const pg::Runtime::Buffer &graph_buffer = state.get_buffer(output_sdf_buffer_index);

			// Apply strength and graph scale. Input serves as output too, shouldn't overlap
			for (unsigned int i = 0; i < in_sdf.size(); ++i) {
				in_sdf[i] = Math::lerp(in_sdf[i], graph_buffer.data[i] * graph_scale, op_strength);
			}
		}
	}

	scale_and_store_sdf(buffer, in_sdf_full);

	data.paste(box.pos, buffer, 1 << channel_index, false);

	_post_edit(box);
}

void VoxelToolLodTerrain::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_raycast_binary_search_iterations", "iterations"),
			&VoxelToolLodTerrain::set_raycast_binary_search_iterations);
	ClassDB::bind_method(D_METHOD("get_raycast_binary_search_iterations"),
			&VoxelToolLodTerrain::get_raycast_binary_search_iterations);
	ClassDB::bind_method(
			D_METHOD("get_voxel_f_interpolated", "position"), &VoxelToolLodTerrain::get_voxel_f_interpolated);
	ClassDB::bind_method(
			D_METHOD("separate_floating_chunks", "box", "parent_node"), &VoxelToolLodTerrain::separate_floating_chunks);
	ClassDB::bind_method(D_METHOD("do_sphere_async", "center", "radius"), &VoxelToolLodTerrain::do_sphere_async);
	ClassDB::bind_method(
			D_METHOD("stamp_sdf", "mesh_sdf", "transform", "isolevel", "sdf_scale"), &VoxelToolLodTerrain::stamp_sdf);
	ClassDB::bind_method(D_METHOD("do_graph", "graph", "transform", "area_size"), &VoxelToolLodTerrain::do_graph);
	ClassDB::bind_method(D_METHOD("do_hemisphere", "center", "radius", "flat_direction", "smoothness"),
			&VoxelToolLodTerrain::do_hemisphere, DEFVAL(0.0));
}

} // namespace zylann::voxel
