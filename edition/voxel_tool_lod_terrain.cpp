#include "voxel_tool_lod_terrain.h"
#include "../constants/voxel_string_names.h"
#include "../generators/graph/voxel_generator_graph.h"
#include "../meshers/blocky/voxel_mesher_blocky.h"
#include "../storage/voxel_buffer_gd.h"
#include "../storage/voxel_data_grid.h"
#include "../terrain/variable_lod/voxel_lod_terrain.h"
#include "../util/containers/std_vector.h"
#include "../util/dstack.h"
#include "../util/island_finder.h"
#include "../util/math/conv.h"
#include "../util/string/format.h"
#include "../util/tasks/async_dependency_tracker.h"
#include "../util/voxel_raycast.h"
#include "floating_chunks.h"
#include "funcs.h"
#include "raycast.h"
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

Ref<VoxelRaycastResult> VoxelToolLodTerrain::raycast(
		Vector3 pos,
		Vector3 dir,
		float max_distance,
		uint32_t collision_mask
) {
	return raycast_generic_world(
			_terrain->get_storage(),
			_terrain->get_mesher(),
			_terrain->get_global_transform(),
			pos,
			dir,
			max_distance,
			collision_mask,
			_raycast_binary_search_iterations
	);
}

void VoxelToolLodTerrain::do_box(Vector3i begin, Vector3i end) {
	ZN_PROFILE_SCOPE();
	ERR_FAIL_COND(_terrain == nullptr);

	ops::DoShapeChunked<ops::SdfAxisAlignedBox, ops::VoxelDataGridAccess> op;
	op.shape.center = to_vec3f(begin + end) * 0.5f;
	op.shape.half_size = to_vec3f(end - begin) * 0.5f;
	op.shape.sdf_scale = get_sdf_scale();
	op.box = op.shape.get_box().clipped(_terrain->get_voxel_bounds());
	op.mode = static_cast<ops::Mode>(get_mode());
	op.texture_params = _texture_params;
	op.blocky_value = _value;
	op.channel = get_channel();
	op.strength = get_sdf_strength();

	if (!is_area_editable(op.box)) {
		ZN_PRINT_WARNING("Area not editable");
		return;
	}

	VoxelData &data = _terrain->get_storage();

	data.pre_generate_box(op.box);

	VoxelDataGrid grid;
	data.get_blocks_grid(grid, op.box, 0);
	op.block_access.grid = &grid;

	{
		VoxelDataGrid::LockWrite wlock(grid);
		op();
	}

	_post_edit(op.box);
}

void VoxelToolLodTerrain::do_sphere(Vector3 center, float radius) {
	ZN_PROFILE_SCOPE();
	ERR_FAIL_COND(_terrain == nullptr);

	ops::DoSphere op;
	op.shape.center = to_vec3f(center);
	op.shape.radius = radius;
	op.shape.sdf_scale = get_sdf_scale();
	op.box = op.shape.get_box().clipped(_terrain->get_voxel_bounds());
	op.mode = static_cast<ops::Mode>(get_mode());
	op.texture_params = _texture_params;
	op.blocky_value = _value;
	op.channel = get_channel();
	op.strength = get_sdf_strength();

	const Box3i world_box = op.box;

	if (!is_area_editable(world_box)) {
		ZN_PRINT_WARNING("Area not editable");
		return;
	}

	VoxelData &data = _terrain->get_storage();

	data.pre_generate_box(world_box);
	data.get_blocks_grid(op.blocks, world_box, 0);

	// We can use floats by doing the operation in local space
	const Vector3i origin_in_voxels = op.blocks.get_origin_block_position_in_voxels();
	op.shape.center = to_vec3f(center - to_vec3(origin_in_voxels));
	op.box.position -= origin_in_voxels;
	op.blocks.use_relative_coordinates();

	op();

	_post_edit(world_box);
}

void VoxelToolLodTerrain::do_hemisphere(Vector3 center, float radius, Vector3 flat_direction, float smoothness) {
	ZN_PROFILE_SCOPE();
	ERR_FAIL_COND(_terrain == nullptr);

	ops::DoShapeChunked<ops::SdfHemisphere, ops::VoxelDataGridAccess> op;
	op.shape.center = to_vec3f(center);
	op.shape.radius = radius;
	op.shape.flat_direction = to_vec3f(flat_direction);
	op.shape.plane_d = flat_direction.dot(center);
	op.shape.smoothness = smoothness;
	op.shape.sdf_scale = get_sdf_scale();
	op.box = op.shape.get_box().clipped(_terrain->get_voxel_bounds());
	op.mode = static_cast<ops::Mode>(get_mode());
	op.texture_params = _texture_params;
	op.blocky_value = _value;
	op.channel = get_channel();
	op.strength = get_sdf_strength();

	if (!is_area_editable(op.box)) {
		ZN_PRINT_WARNING("Area not editable");
		return;
	}

	VoxelData &data = _terrain->get_storage();

	data.pre_generate_box(op.box);

	VoxelDataGrid grid;
	data.get_blocks_grid(grid, op.box, 0);
	op.block_access.grid = &grid;

	{
		VoxelDataGrid::LockWrite wlock(grid);
		op();
	}

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

	void run(ThreadedTaskContext &ctx) override {
		ZN_PROFILE_SCOPE();
		ZN_ASSERT(_data != nullptr);
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
	op.shape.center = to_vec3f(center);
	op.shape.radius = radius;
	op.shape.sdf_scale = get_sdf_scale();
	op.box = op.shape.get_box().clipped(_terrain->get_voxel_bounds());
	op.mode = ops::Mode(get_mode());
	op.texture_params = _texture_params;
	op.blocky_value = _value;
	op.channel = get_channel();
	op.strength = get_sdf_strength();

	if (!is_area_editable(op.box)) {
		ZN_PRINT_WARNING("Area not editable");
		return;
	}

	std::shared_ptr<VoxelData> data = _terrain->get_storage_shared();

	VoxelToolAsyncEdit<ops::DoSphere> *task = ZN_NEW(VoxelToolAsyncEdit<ops::DoSphere>(op, data));
	_terrain->push_async_edit(task, op.box, task->get_tracker());
}

void VoxelToolLodTerrain::copy(Vector3i pos, VoxelBuffer &dst, uint8_t channels_mask) const {
	ERR_FAIL_COND(_terrain == nullptr);
	if (channels_mask == 0) {
		channels_mask = (1 << _channel);
	}
	_terrain->get_storage().copy(pos, dst, channels_mask);
}

void VoxelToolLodTerrain::paste(Vector3i pos, const VoxelBuffer &src, uint8_t channels_mask) {
	ERR_FAIL_COND(_terrain == nullptr);
	if (channels_mask == 0) {
		channels_mask = (1 << _channel);
	}
	const Box3i box(pos, src.get_size());
	if (!is_area_editable(box)) {
		ZN_PRINT_WARNING("Area not editable");
		return;
	}

	VoxelData &data = _terrain->get_storage();

	data.pre_generate_box(box);
	data.paste(pos, src, channels_mask, false);

	_post_edit(box);
}

float VoxelToolLodTerrain::get_voxel_f_interpolated(Vector3 position) const {
	ZN_PROFILE_SCOPE();
	ERR_FAIL_COND_V(_terrain == nullptr, 0);
	const int channel = get_channel();
	VoxelData &data = _terrain->get_storage();
	// TODO Optimization: is it worth making a fast-path for this?
	return get_sdf_interpolated(
			[&data, channel](Vector3i ipos) {
				VoxelSingleValue defval;
				defval.f = constants::SDF_FAR_OUTSIDE;
				VoxelSingleValue value = data.get_voxel(ipos, channel, defval);
				return value.f;
			},
			position
	);
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
	defval.f = constants::SDF_FAR_OUTSIDE;
	return _terrain->get_storage().get_voxel(pos, _channel, defval).f;
}

void VoxelToolLodTerrain::_set_voxel(Vector3i pos, uint64_t v) {
	ERR_FAIL_COND(_terrain == nullptr);
	_terrain->get_storage().try_set_voxel(v, pos, _channel);
	// No post_update, the parent class does it, it's a generic slow implementation.
}

void VoxelToolLodTerrain::_set_voxel_f(Vector3i pos, float v) {
	ERR_FAIL_COND(_terrain == nullptr);
	// TODO Format should be accessible from terrain
	_terrain->get_storage().try_set_voxel_f(v, pos, _channel);
	// No post_update, the parent class does it, it's a generic slow implementation.
}

void VoxelToolLodTerrain::_post_edit(const Box3i &box) {
	ERR_FAIL_COND(_terrain == nullptr);
	_terrain->post_edit_area(box, true);
}

int VoxelToolLodTerrain::get_raycast_binary_search_iterations() const {
	return _raycast_binary_search_iterations;
}

void VoxelToolLodTerrain::set_raycast_binary_search_iterations(int iterations) {
	_raycast_binary_search_iterations = math::clamp(iterations, 0, 16);
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
			*this, int_world_box, parent_node, _terrain->get_global_transform(), mesher, materials
	);
}

// Combines a precalculated SDF with the terrain at a specific position, rotation and scale.
//
// `transform` is where the buffer should be applied on the terrain.
//
// `isolevel` alters the shape of the SDF: positive "puffs" it, negative "erodes" it. This is a applied after
// `sdf_scale`.
//
// `sdf_scale` scales SDF values (it doesn't make the shape bigger or smaller). Usually defaults to 1 but may be lower
// if artifacts show up due to scaling used in terrain SDF.
//
void VoxelToolLodTerrain::stamp_sdf(
		Ref<VoxelMeshSDF> mesh_sdf,
		Transform3D transform,
		float isolevel,
		float sdf_scale
) {
	// TODO Asynchronous version
	ZN_PROFILE_SCOPE();

	ERR_FAIL_COND(_terrain == nullptr);
	ERR_FAIL_COND(mesh_sdf.is_null());
	ERR_FAIL_COND(!mesh_sdf->is_baked());
	Ref<godot::VoxelBuffer> buffer_ref = mesh_sdf->get_voxel_buffer();
	ERR_FAIL_COND(buffer_ref.is_null());
	const VoxelBuffer &buffer = buffer_ref->get_buffer();
	const VoxelBuffer::ChannelId channel = VoxelBuffer::CHANNEL_SDF;
	ERR_FAIL_COND(buffer.get_channel_compression(channel) == VoxelBuffer::COMPRESSION_UNIFORM);
	ERR_FAIL_COND(buffer.get_channel_depth(channel) != VoxelBuffer::DEPTH_32_BIT);

	const Transform3D &box_to_world = transform;
	const AABB local_aabb = mesh_sdf->get_aabb();

	// Note, transform is local to the terrain
	const AABB aabb = box_to_world.xform(local_aabb);
	const Box3i voxel_box = Box3i::from_min_max(aabb.position.floor(), (aabb.position + aabb.size).ceil());

	// TODO Sometimes it will fail near unloaded blocks, even though the transformed box does not intersect them.
	// This could be avoided with a box/transformed-box intersection algorithm. Might investigate if the use case
	// occurs. It won't happen with full load mode. This also affects other shapes.
	if (!is_area_editable(voxel_box)) {
		ZN_PRINT_WARNING("Area not editable");
		return;
	}

	VoxelData &data = _terrain->get_storage();

	data.pre_generate_box(voxel_box);

	// TODO Maybe more efficient to "rasterize" the box? We're going to iterate voxels the box doesn't intersect.
	// TODO Maybe we should scale SDF values based on the scale of the transform too

	const Transform3D buffer_to_box =
			Transform3D(Basis().scaled(Vector3(local_aabb.size / buffer.get_size())), local_aabb.position);
	const Transform3D buffer_to_world = box_to_world * buffer_to_box;

	// TODO Support other depths, format should be accessible from the volume
	ops::SdfOperation16bit<ops::SdfUnion, ops::SdfBufferShape> op;
	op.op.strength = get_sdf_strength();
	op.shape.world_to_buffer = buffer_to_world.affine_inverse();
	op.shape.buffer_size = buffer.get_size();
	op.shape.isolevel = isolevel;
	op.shape.sdf_scale = sdf_scale;
	// Note, the passed buffer must not be shared with another thread.
	// buffer.decompress_channel(channel);
	ZN_ASSERT_RETURN(buffer.get_channel_data_read_only(channel, op.shape.buffer));

	VoxelDataGrid grid;
	data.get_blocks_grid(grid, voxel_box, 0);
	grid.write_box(voxel_box, VoxelBuffer::CHANNEL_SDF, op);

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

	const Box3i box = Box3i::from_min_max(
							  math::floor_to_int(transform.origin - 0.5 * area_size),
							  math::ceil_to_int(transform.origin + 0.5 * area_size)
	)
							  .padded(2)
							  .clipped(_terrain->get_voxel_bounds());

	if (!is_area_editable(box)) {
		ZN_PRINT_WARNING("Area not editable");
		return;
	}

	VoxelData &data = _terrain->get_storage();

	data.pre_generate_box(box);

	const unsigned int channel_index = VoxelBuffer::CHANNEL_SDF;

	VoxelBuffer buffer(VoxelBuffer::ALLOCATOR_POOL);
	buffer.create(box.size);
	data.copy(box.position, buffer, 1 << channel_index);

	buffer.decompress_channel(channel_index);

	// Convert input SDF
	static thread_local StdVector<float> tls_in_sdf_full;
	tls_in_sdf_full.resize(Vector3iUtil::get_volume_u64(buffer.get_size()));
	Span<float> in_sdf_full = to_span(tls_in_sdf_full);
	get_unscaled_sdf(buffer, in_sdf_full);

	static thread_local StdVector<float> tls_in_x;
	static thread_local StdVector<float> tls_in_y;
	static thread_local StdVector<float> tls_in_z;
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

	for (float &sd : in_sdf_full) {
		sd *= inv_graph_scale;
	}

	const float op_strength = get_sdf_strength();

	{
		ZN_PROFILE_SCOPE_NAMED("Slices");
		// For each deck of the box (doing this to reduce memory usage since the graph will allocate temporary buffers
		// for each operation, which can be a lot depending on the complexity of the graph)
		Vector3i pos;
		const Vector3i endpos = box.position + box.size;
		for (pos.z = box.position.z; pos.z < endpos.z; ++pos.z) {
			// Set positions
			for (unsigned int i = 0; i < deck_area; ++i) {
				in_z[i] = pos.z;
			}
			{
				unsigned int i = 0;
				for (pos.x = box.position.x; pos.x < endpos.x; ++pos.x) {
					for (pos.y = box.position.y; pos.y < endpos.y; ++pos.y) {
						in_x[i] = pos.x;
						in_y[i] = pos.y;
						++i;
					}
				}
			}

			// Transform positions to be local to the graph
			for (unsigned int i = 0; i < deck_area; ++i) {
				Vector3 graph_local_pos(in_x[i], in_y[i], in_z[i]);
				graph_local_pos = inv_transform.xform(graph_local_pos);
				in_x[i] = graph_local_pos.x;
				in_y[i] = graph_local_pos.y;
				in_z[i] = graph_local_pos.z;
			}

			// Get SDF input
			Span<float> in_sdf = in_sdf_full.sub(deck_area * (pos.z - box.position.z), deck_area);

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

	data.paste(box.position, buffer, 1 << channel_index, false);

	_post_edit(box);
}

void VoxelToolLodTerrain::run_blocky_random_tick(
		const AABB voxel_area,
		const int voxel_count,
		const Callable &callback,
		const int block_batch_count
) {
	ZN_PROFILE_SCOPE();

	ZN_ASSERT_RETURN(_terrain != nullptr);

	Ref<VoxelMesherBlocky> mesher = _terrain->get_mesher();
	ZN_ASSERT_RETURN_MSG(
			mesher.is_valid(),
			format("This function requires a volume using {} with a valid library", ZN_CLASS_NAME_C(VoxelMesherBlocky))
	);
	Ref<VoxelBlockyLibraryBase> library = mesher->get_library();
	ZN_ASSERT_RETURN_MSG(library.is_valid(), format("{} has no library assigned", ZN_CLASS_NAME_C(VoxelMesherBlocky)));

	ZN_ASSERT_RETURN(callback.is_valid());
	ZN_ASSERT_RETURN(block_batch_count > 0);
	ZN_ASSERT_RETURN(voxel_count >= 0);
	ZN_ASSERT_RETURN(math::is_valid_size(voxel_area.size));

	if (voxel_count == 0) {
		return;
	}

	VoxelData &data = _terrain->get_storage();

	zylann::voxel::run_blocky_random_tick(
			data, voxel_area, **library, _random, voxel_count, block_batch_count, callback
	);
}

void VoxelToolLodTerrain::_bind_methods() {
	using Self = VoxelToolLodTerrain;

	ClassDB::bind_method(
			D_METHOD("set_raycast_binary_search_iterations", "iterations"), &Self::set_raycast_binary_search_iterations
	);
	ClassDB::bind_method(D_METHOD("get_raycast_binary_search_iterations"), &Self::get_raycast_binary_search_iterations);
	ClassDB::bind_method(D_METHOD("get_voxel_f_interpolated", "position"), &Self::get_voxel_f_interpolated);
	ClassDB::bind_method(D_METHOD("separate_floating_chunks", "box", "parent_node"), &Self::separate_floating_chunks);
	ClassDB::bind_method(D_METHOD("do_sphere_async", "center", "radius"), &Self::do_sphere_async);
	ClassDB::bind_method(D_METHOD("stamp_sdf", "mesh_sdf", "transform", "isolevel", "sdf_scale"), &Self::stamp_sdf);
	ClassDB::bind_method(D_METHOD("do_graph", "graph", "transform", "area_size"), &Self::do_graph);
	ClassDB::bind_method(
			D_METHOD("do_hemisphere", "center", "radius", "flat_direction", "smoothness"),
			&Self::do_hemisphere,
			DEFVAL(0.0)
	);
	ClassDB::bind_method(
			D_METHOD("run_blocky_random_tick", "area", "voxel_count", "callback", "batch_count"),
			&Self::run_blocky_random_tick,
			DEFVAL(16)
	);
}

} // namespace zylann::voxel
