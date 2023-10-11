#include "voxel_generator_multipass_cb.h"
#include "../../engine/voxel_engine.h"
#include "../../util/godot/core/array.h"
#include "../../util/profiling.h"
#include "../../util/string_funcs.h"

#include "../../util/dstack.h"
#include "../../util/godot/classes/time.h"
#include "../../util/noise/fast_noise_lite/fast_noise_lite.h"

namespace zylann::voxel {

VoxelGeneratorMultipassCB::VoxelGeneratorMultipassCB() {
	std::shared_ptr<Internal> internal = make_shared_instance<Internal>();

	// PLACEHOLDER
	{
		Pass pass;
		internal->passes.push_back(pass);
	}
	{
		Pass pass;
		pass.dependency_extents = 1;
		internal->passes.push_back(pass);
	}
	{
		Pass pass;
		pass.dependency_extents = 1;
		internal->passes.push_back(pass);
	}

	// MutexLock mlock(_internal_mutex);
	_internal = internal;
}

VoxelGeneratorMultipassCB::~VoxelGeneratorMultipassCB() {}

VoxelGeneratorMultipassCB::Map::~Map() {
	// If the map gets destroyed then we know the last reference to it was removed, which means only one thread had
	// access to it, so we can get away not locking anything if cleanup is needed.

	// Initially I thought this is where we'd re-schedule any task pointers still in blocks, but that can't happen,
	// because if tasks exist referencing the multipass generator, then the generator (and so the map) would not be
	// destroyed, and so we wouldn't be here.
	// Therefore there is only just an assert in ~Block to ensure the only other case where blocks are removed from the
	// map before handling these tasks, or to warn us in case there is a case we didn't expect.
	//
	// That said, that pretty much describes a cyclic reference. This cycle is usually broken by VoxelTerrain, which
	// controls the streaming behavior. If a terrain gets destroyed, it must tell the generator to unload its cache.
}

VoxelGenerator::Result VoxelGeneratorMultipassCB::generate_block(VoxelQueryData &input) {
	const int bs = input.voxel_buffer.get_size().y;

	std::shared_ptr<Internal> internal = get_internal();

	if (input.origin_in_voxels.y + bs <= internal->column_base_y_blocks * bs) {
		// TODO Generate bottom fallback

	} else if (input.origin_in_voxels.y >= (internal->column_base_y_blocks + internal->column_height_blocks) * bs) {
		// TODO Generate top fallback

	} else {
		// Can't generate column chunks from here for now
		// TODO Fallback on an expensive single-threaded dependency generation, which we might throw away after
		ZN_PRINT_ERROR("Not implemented");
	}

	return { false };
}

int VoxelGeneratorMultipassCB::get_pass_count() const {
	return get_internal()->passes.size();
}

void VoxelGeneratorMultipassCB::set_pass_count(int pass_count) {
	if (get_pass_count() == pass_count) {
		return;
	}

	ZN_ASSERT_RETURN_MSG(
			pass_count > 0 && pass_count <= MAX_PASSES, format("Pass count is limited from {} to {}", 1, MAX_PASSES));

	reset_internal([pass_count](Internal &internal) { //
		internal.passes.resize(pass_count, Pass{ 1 });
	});
}

int VoxelGeneratorMultipassCB::get_column_base_y_blocks() const {
	return get_internal()->column_base_y_blocks;
}

void VoxelGeneratorMultipassCB::set_column_base_y_blocks(int new_y) {
	if (get_column_base_y_blocks() == new_y) {
		return;
	}
	reset_internal([new_y](Internal &internal) { //
		internal.column_base_y_blocks = new_y;
	});
}

int VoxelGeneratorMultipassCB::get_column_height_blocks() const {
	return get_internal()->column_height_blocks;
}

void VoxelGeneratorMultipassCB::set_column_height_blocks(int new_height) {
	new_height = math::clamp(new_height, 0, MAX_COLUMN_HEIGHT_BLOCKS);
	if (get_column_height_blocks() == new_height) {
		return;
	}
	reset_internal([new_height](Internal &internal) { //
		internal.column_height_blocks = new_height;
	});
}

int VoxelGeneratorMultipassCB::get_pass_extent_blocks(int pass_index) const {
	std::shared_ptr<Internal> internal = get_internal();
	ZN_ASSERT_RETURN_V(pass_index >= 0 && pass_index < int(internal->passes.size()), 0);
	return internal->passes[pass_index].dependency_extents;
}

void VoxelGeneratorMultipassCB::set_pass_extent_blocks(int pass_index, int new_extent) {
	std::shared_ptr<Internal> internal = get_internal();
	ZN_ASSERT_RETURN(pass_index >= 0 && pass_index < int(internal->passes.size()));

	if (pass_index == 0) {
		ZN_ASSERT_RETURN_MSG(new_extent == 0, "Non-zero extents is not supported for the first pass.");
		return;

	} else {
		ZN_ASSERT_RETURN_MSG(new_extent >= 1 && new_extent <= MAX_PASS_EXTENT,
				format("Pass extents are limited between {} and {}.", 1, MAX_PASS_EXTENT));
	}

	reset_internal([pass_index, new_extent](Internal &internal) { //
		internal.passes[pass_index].dependency_extents = new_extent;
	});
}

// Internal

std::shared_ptr<VoxelGeneratorMultipassCB::Internal> VoxelGeneratorMultipassCB::get_internal() const {
	MutexLock mlock(_internal_mutex);
	ZN_ASSERT(_internal != nullptr);
	return _internal;
}

namespace {
// TODO Placeholder utility to emulate what would be the final use case
struct GridEditor {
	Span<VoxelGeneratorMultipassCB::Block *> blocks;
	Box3i world_box;

	inline VoxelGeneratorMultipassCB::Block *get_block_and_relative_position(
			Vector3i voxel_pos_world, Vector3i &voxel_pos_block) {
		const Vector3i block_pos_world = voxel_pos_world >> 4;
		ZN_ASSERT(world_box.contains(block_pos_world));
		const Vector3i block_pos_grid = block_pos_world - world_box.pos;
		const unsigned int grid_loc = Vector3iUtil::get_zxy_index(block_pos_grid, world_box.size);
		VoxelGeneratorMultipassCB::Block *block = blocks[grid_loc];
		ZN_ASSERT(block != nullptr);
		voxel_pos_block = voxel_pos_world & 15;
		return block;
	}

	int get_voxel(Vector3i voxel_pos_world, int value, int channel) {
		Vector3i voxel_pos_block;
		VoxelGeneratorMultipassCB::Block *block = get_block_and_relative_position(voxel_pos_world, voxel_pos_block);
		return block->voxels.get_voxel(voxel_pos_block, channel);
	}

	void set_voxel(Vector3i voxel_pos_world, int value, int channel) {
		Vector3i voxel_pos_block;
		VoxelGeneratorMultipassCB::Block *block = get_block_and_relative_position(voxel_pos_world, voxel_pos_block);
		block->voxels.set_voxel(value, voxel_pos_block, channel);
	}
};

int get_total_dependency_extent(const VoxelGeneratorMultipassCB::Internal &generator) {
	int extent = 0;
	for (unsigned int pass_index = 1; pass_index < generator.passes.size(); ++pass_index) {
		const VoxelGeneratorMultipassCB::Pass &pass = generator.passes[pass_index];
		if (pass.dependency_extents == 0) {
			ZN_PRINT_ERROR("Unexpected pass dependency extents");
		}
		extent += pass.dependency_extents * 2;
	}
	return extent;
}

} // namespace

void VoxelGeneratorMultipassCB::generate_pass(PassInput input) {
	// PLACEHOLDER
	// Note: must not access _internal from here, only use `input`

	const Vector3i column_origin_in_voxels = input.main_block_position * 16;
	static const VoxelBufferInternal::ChannelId channel = VoxelBufferInternal::CHANNEL_TYPE;

	if (input.pass_index == 0) {
		GridEditor editor;
		editor.blocks = input.grid;
		editor.world_box = Box3i(input.grid_origin, input.grid_size);

		Ref<ZN_FastNoiseLite> fnl;
		fnl.instantiate();

		for (int rby = 0; rby < input.grid_size.y; ++rby) {
			Block *block = input.grid[rby];
			ZN_ASSERT(block != nullptr);
			VoxelBufferInternal &voxels = block->voxels;
			Vector3i rpos;

			const Vector3i bs = voxels.get_size();
			const Vector3i block_origin_in_voxels = column_origin_in_voxels + Vector3i(0, rby * input.block_size, 0);

			for (rpos.z = 0; rpos.z < bs.z; ++rpos.z) {
				for (rpos.x = 0; rpos.x < bs.x; ++rpos.x) {
					for (rpos.y = 0; rpos.y < bs.y; ++rpos.y) {
						Vector3i pos = block_origin_in_voxels + rpos;
						const float sd = pos.y + 5.f * fnl->get_noise_2d(pos.x, pos.z);

						int v = 0;
						if (sd < 0.f) {
							v = 1;
						}

						voxels.set_voxel(v, rpos, channel);
					}
				}
			}

			voxels.compress_uniform_channels();
		}

	} else if (input.pass_index == 1) {
		GridEditor editor;
		editor.blocks = input.grid;
		editor.world_box = Box3i(input.grid_origin, input.grid_size);

		const Box3i column_box(input.grid_origin * 16, input.grid_size * 16);

		for (int rby = 0; rby < input.grid_size.y; ++rby) {
			const Vector3i block_origin_in_voxels = column_origin_in_voxels + Vector3i(0, rby * input.block_size, 0);

			const uint32_t h = Variant(block_origin_in_voxels).hash();

			Vector3i structure_center;
			structure_center.x = h & 15;
			structure_center.y = (h >> 4) & 15;
			structure_center.z = (h >> 8) & 15;
			const int structure_size = (h >> 12) & 15;

			const Box3i structure_box =
					Box3i(structure_center - Vector3iUtil::create(structure_size / 2) + block_origin_in_voxels,
							Vector3iUtil::create(structure_size))
							.clipped(column_box);

			structure_box.for_each_cell_zxy([&editor](Vector3i pos) { //
				editor.set_voxel(pos, 1, channel);
			});
		}

	} else if (input.pass_index == 2) {
		// blep
		Thread::sleep_usec(1000);
	}
}

namespace {

inline Vector2i to_vec2i_xz(Vector3i p) {
	return Vector2i(p.x, p.z);
}

Box2i to_box2i_in_height_range(Box3i box3, int min_y, int height) {
	if (box3.pos.y + box3.size.y <= min_y || box3.pos.y >= min_y + height) {
		// Empty box because it doesn't intersect the height range
		return Box2i(to_vec2i_xz(box3.pos), Vector2i());
	}
	return Box2i(to_vec2i_xz(box3.pos), to_vec2i_xz(box3.size));
}

} // namespace

void VoxelGeneratorMultipassCB::re_initialize_column_refcounts() {
	// This should only be called following a map reset
	ZN_ASSERT_RETURN_MSG(get_internal()->map.columns.size() == 0, "Bug!");

	for (PairedViewer &pv : _paired_viewers) {
		process_viewer_diff_internal(pv.request_box, Box3i());
	}
}

void VoxelGeneratorMultipassCB::process_viewer_diff(ViewerID id, Box3i p_requested_box, Box3i p_prev_requested_box) {
	PairedViewer *paired_viewer = nullptr;
	for (PairedViewer &pv : _paired_viewers) {
		if (pv.id == id) {
			paired_viewer = &pv;
			break;
		}
	}
	if (paired_viewer == nullptr) {
		// This viewer wasn't known to the generator before. Could mean it just got paired, or properties of the
		// generator were changed (which can cause the generator's internal state and viewers to be reset)
		_paired_viewers.push_back(PairedViewer{ id, p_requested_box });
		// Force updating the whole area by simulating what happens when a viewer gets paired.
		// p_prev_requested_box = Box3i();

	} else {
		paired_viewer->request_box = p_requested_box;
	}

	process_viewer_diff_internal(p_requested_box, p_prev_requested_box);
}

void VoxelGeneratorMultipassCB::process_viewer_diff_internal(Box3i p_requested_box, Box3i p_prev_requested_box) {
	ZN_DSTACK();
	ZN_PROFILE_SCOPE();
	// TODO Could run as a task similarly to threaded update of VLT
	// However if we do that we need to make sure block requests dont end up cancelled due to no block being found to
	// load... the easiest way I can think of, is to just run this in the same thread that triggers the requests, and
	// that means moving VoxelTerrain's process to a thread as well.

	std::shared_ptr<Internal> internal = get_internal();

	static const int block_size = 1 << constants::DEFAULT_BLOCK_SIZE_PO2;

	const int total_extent = get_total_dependency_extent(*internal);

	const Box2i requested_box_2d =
			to_box2i_in_height_range(p_requested_box, internal->column_base_y_blocks, internal->column_height_blocks);
	const Box2i prev_requested_box_2d = to_box2i_in_height_range(
			p_prev_requested_box, internal->column_base_y_blocks, internal->column_height_blocks);

	// println(format("R {} {} {} {} {}", requested_box_2d.pos.x, requested_box_2d.pos.y, requested_box_2d.size.x,
	// 		requested_box_2d.size.y, Time::get_singleton()->get_ticks_usec()));

	// Note: empty boxes should not be padded, they mean nothing is requested, so the padded request must also
	// be empty.
	const Box2i load_requested_box =
			requested_box_2d.is_empty() ? requested_box_2d : requested_box_2d.padded(total_extent);
	const Box2i prev_load_requested_box =
			prev_requested_box_2d.is_empty() ? prev_requested_box_2d : prev_requested_box_2d.padded(total_extent);

	// println(format("L {} {} {} {} {}", load_requested_box.pos.x, load_requested_box.pos.y, load_requested_box.size.x,
	// 		load_requested_box.size.y, Time::get_singleton()->get_ticks_usec()));

	Map &map = internal->map;

	BufferedTaskScheduler &task_scheduler = BufferedTaskScheduler::get_for_current_thread();

	// Blocks to view
	const int column_height = internal->column_height_blocks;
	load_requested_box.difference(prev_load_requested_box, [&map, column_height](Box2i new_box) {
		{
			SpatialLock2D::Write swlock(map.spatial_lock, new_box);
			MutexLock mlock(map.mutex);

			new_box.for_each_cell_yx([&map, column_height](Vector2i bpos) {
				Column &column = map.columns[bpos];
				if (column.blocks.size() == 0) {
					column.blocks.resize(column_height);
				}
				// if (block == nullptr) {
				// 	block = make_unique_instance<Block>();
				// 	// block->loading = true;
				// }
				column.viewers.add();
			});

			// TODO Implement loading tasks
		}
	});

	// Blocks to unview
	prev_load_requested_box.difference(load_requested_box, [&map, &task_scheduler](Box2i old_box) {
		{
			SpatialLock2D::Write swlock(map.spatial_lock, old_box);
			MutexLock mlock(map.mutex);

			old_box.for_each_cell_yx([&map, &task_scheduler](Vector2i cpos) {
				auto it = map.columns.find(cpos);

				// The block must be found because last time the block was in the loading area of the viewer.
				ZN_ASSERT(it != map.columns.end());
				Column &column = it->second;

				column.viewers.remove();
				if (column.viewers.get() == 0) {
					for (Block &block : column.blocks) {
						if (block.final_pending_task != nullptr) {
							// There was a pending generate task, resume it, but it should basically return a drop.
							// (also because we are locking the map, that task must not run until we're done removing
							// its target column)
							task_scheduler.push_main_task(block.final_pending_task);
							block.final_pending_task = nullptr;
						}
					}

					// TODO Implement saving tasks
					// We remove immediately for now
					map.columns.erase(it);
					// println(format("U {} {} {} {} {}", 0, cpos.x, 0, cpos.y,
					// Time::get_singleton()->get_ticks_usec()));
				}
			});
		}
	});

	task_scheduler.flush();
}

void VoxelGeneratorMultipassCB::clear_cache() {
	reset_internal([](const Internal &) {});

	// This might lock up for a few seconds if the generator is busy
	/*
	Map &map = old_internal->map;
	SpatialLock2D::Write swlock(map.spatial_lock, BoxBounds2i::from_everywhere());
	MutexLock mlock(map.mutex);

	BufferedTaskScheduler &task_scheduler = BufferedTaskScheduler::get_for_current_thread();

	for (auto it = map.columns.begin(); it != map.columns.end(); ++it) {
		Column &column = it->second;

		for (Block &block : column.blocks) {
			// Kick tasks out of here
			if (block.final_pending_task != nullptr) {
				task_scheduler.push_main_task(block.final_pending_task);
				block.final_pending_task = nullptr;
			}
		}
	}
	map.columns.clear();
	*/
}

bool VoxelGeneratorMultipassCB::debug_try_get_column_states(std::vector<DebugColumnState> &out_states) {
	ZN_PROFILE_SCOPE();

	out_states.clear();

	std::shared_ptr<Internal> internal = get_internal();
	Map &map = internal->map;

	{
		unsigned int size = 0;
		{
			MutexLock mlock(map.mutex);
			size = map.columns.size();
		}
		out_states.reserve(size);
	}

	if (!map.spatial_lock.try_lock_read(BoxBounds2i::from_everywhere())) {
		// Don't hang here on the main thread, while generating it's very likely the map is locked somewhere.
		// We can poll this function regularly from a debug tool until locking succeeds.
		return false;
	}
	SpatialLock2D::UnlockReadOnScopeExit srlock(map.spatial_lock, BoxBounds2i::from_everywhere());

	MutexLock mlock(map.mutex);

	for (auto it = map.columns.begin(); it != map.columns.end(); ++it) {
		Column &column = it->second;
		out_states.push_back(DebugColumnState{ it->first, column.subpass_index, uint8_t(column.viewers.get()) });
	}

	return true;
}

// BINDING LAND

bool VoxelGeneratorMultipassCB::_set(const StringName &p_name, const Variant &p_value) {
	const String property_name = p_name;

	if (property_name.begins_with("pass_")) {
		const int index_pos = ZN_ARRAY_LENGTH("pass_") - 1; // -1 to exclude the '\0'
		const int index_end_pos = property_name.find("_", index_pos);
		const int index = property_name.substr(index_pos, index_end_pos - index_pos).to_int();

		String sub_property_name = property_name.substr(index_end_pos);
		if (sub_property_name == "_extent") {
			set_pass_extent_blocks(index, p_value);
			return true;
		}
	}

	return false;
}

bool VoxelGeneratorMultipassCB::_get(const StringName &p_name, Variant &r_ret) const {
	const String property_name = p_name;

	if (property_name.begins_with("pass_")) {
		const int index_pos = ZN_ARRAY_LENGTH("pass_") - 1; // -1 to exclude the '\0'
		const int index_end_pos = property_name.find("_", index_pos);
		const int index = property_name.substr(index_pos, index_end_pos - index_pos).to_int();

		String sub_property_name = property_name.substr(index_end_pos);
		if (sub_property_name == "_extent") {
			r_ret = get_pass_extent_blocks(index);
			return true;
		}
	}

	return false;
}

void VoxelGeneratorMultipassCB::_get_property_list(List<PropertyInfo> *p_list) const {
	std::shared_ptr<Internal> internal = get_internal();
	String pass_extent_range_hint_string = String("{0},{1}").format(varray(1, MAX_PASS_EXTENT));

	for (unsigned int pass_index = 0; pass_index < internal->passes.size(); ++pass_index) {
		String pname = String("pass_{0}_extent").format(varray(pass_index));

		if (pass_index == 0) {
			p_list->push_back(PropertyInfo(
					Variant::INT, pname, PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_READ_ONLY));
		} else {
			p_list->push_back(PropertyInfo(Variant::INT, pname, PROPERTY_HINT_RANGE, pass_extent_range_hint_string));
		}
	}
}

void VoxelGeneratorMultipassCB::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_pass_count"), &VoxelGeneratorMultipassCB::get_pass_count);
	ClassDB::bind_method(D_METHOD("set_pass_count", "count"), &VoxelGeneratorMultipassCB::set_pass_count);

	ClassDB::bind_method(
			D_METHOD("get_pass_extent_blocks", "pass_index"), &VoxelGeneratorMultipassCB::get_pass_extent_blocks);
	ClassDB::bind_method(D_METHOD("set_pass_extent_blocks", "pass_index", "extent"),
			&VoxelGeneratorMultipassCB::set_pass_extent_blocks);

	ClassDB::bind_method(D_METHOD("get_column_base_y_blocks"), &VoxelGeneratorMultipassCB::get_column_base_y_blocks);
	ClassDB::bind_method(
			D_METHOD("set_column_base_y_blocks", "y"), &VoxelGeneratorMultipassCB::set_column_base_y_blocks);

	ClassDB::bind_method(D_METHOD("get_column_height_blocks"), &VoxelGeneratorMultipassCB::get_column_height_blocks);
	ClassDB::bind_method(
			D_METHOD("set_column_height_blocks", "y"), &VoxelGeneratorMultipassCB::set_column_height_blocks);

	ADD_PROPERTY(
			PropertyInfo(Variant::INT, "column_base_y_blocks"), "set_column_base_y_blocks", "get_column_base_y_blocks");

	ADD_PROPERTY(
			PropertyInfo(Variant::INT, "column_height_blocks"), "set_column_height_blocks", "get_column_height_blocks");

	ADD_PROPERTY(PropertyInfo(Variant::INT, "pass_count", PROPERTY_HINT_RANGE,
						 String("{0},{1}").format(varray(1, MAX_PASSES))),
			"set_pass_count", "get_pass_count");

	BIND_CONSTANT(MAX_PASSES);
	BIND_CONSTANT(MAX_PASS_EXTENT);
}

} // namespace zylann::voxel
