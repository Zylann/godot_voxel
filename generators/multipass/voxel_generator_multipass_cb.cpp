#include "voxel_generator_multipass_cb.h"
#include "../../engine/buffered_task_scheduler.h"
#include "../../util/containers/container_funcs.h"
#include "../../util/dstack.h"
#include "../../util/godot/check_ref_ownership.h"
#include "../../util/godot/classes/time.h"
#include "../../util/godot/core/array.h"
#include "../../util/profiling.h"
#include "../../util/string/format.h"
#include "generate_block_multipass_cb_task.h"

namespace zylann::voxel {

using namespace VoxelGeneratorMultipassCBStructs;

VoxelGeneratorMultipassCB::VoxelGeneratorMultipassCB() {
	std::shared_ptr<Internal> internal = make_shared_instance<Internal>();
	// MutexLock mlock(_internal_mutex);
	_internal = internal;
}

VoxelGeneratorMultipassCB::~VoxelGeneratorMultipassCB() {}

VoxelGenerator::Result VoxelGeneratorMultipassCB::generate_block(VoxelQueryData &input) {
	if (input.lod > 0) {
		// Not supported
		return { false };
	}

	const int bs = input.voxel_buffer.get_size().y;

	std::shared_ptr<Internal> internal = get_internal();

	if (input.origin_in_voxels.y + bs <= internal->column_base_y_blocks * bs) {
		generate_block_fallback_script(input);

	} else if (input.origin_in_voxels.y >= (internal->column_base_y_blocks + internal->column_height_blocks) * bs) {
		generate_block_fallback_script(input);

	} else {
		// Can't generate column chunks from here for now
		// TODO Fallback on an expensive single-threaded dependency generation, which we might throw away after?
		// Or trigger a threaded task and block here until it's done?
		ZN_PRINT_ERROR("Not implemented");
	}

	return { false };
}

IThreadedTask *VoxelGeneratorMultipassCB::create_block_task(const VoxelGenerator::BlockTaskParams &params) const {
	return ZN_NEW(GenerateBlockMultipassCBTask(params));
}

void VoxelGeneratorMultipassCB::generate_block_fallback_script(VoxelQueryData &input) {
	if (get_script() == Variant()) {
		return;
	}

	// Create a temporary wrapper so Godot can pass it to scripts
	Ref<godot::VoxelBuffer> buffer_wrapper(
			memnew(godot::VoxelBuffer(static_cast<godot::VoxelBuffer::Allocator>(input.voxel_buffer.get_allocator()))));
	buffer_wrapper.instantiate();
	buffer_wrapper->get_buffer().copy_format(input.voxel_buffer);
	buffer_wrapper->get_buffer().create(input.voxel_buffer.get_size());

	{
		ZN_GODOT_CHECK_REF_COUNT_DOES_NOT_CHANGE(buffer_wrapper);
		GDVIRTUAL_CALL(_generate_block_fallback, buffer_wrapper, input.origin_in_voxels);
	}

	// The wrapper is discarded
	buffer_wrapper->get_buffer().move_to(input.voxel_buffer);
}

int VoxelGeneratorMultipassCB::get_used_channels_mask() const {
	int mask = 0;
	if (!GDVIRTUAL_CALL(_get_used_channels_mask, mask)) {
		// WARN_PRINT_ONCE("VoxelGeneratorScript::_get_used_channels_mask is unimplemented!");
	}
	return mask;
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

	re_initialize_column_refcounts();
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
	re_initialize_column_refcounts();
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
	re_initialize_column_refcounts();
}

int VoxelGeneratorMultipassCB::get_pass_extent_blocks(int pass_index) const {
	std::shared_ptr<Internal> internal = get_internal();
	ZN_ASSERT_RETURN_V(pass_index >= 0 && pass_index < int(internal->passes.size()), 0);
	return internal->passes[pass_index].dependency_extents;
}

void VoxelGeneratorMultipassCB::set_pass_extent_blocks(int pass_index, int new_extent) {
	ZN_ASSERT_RETURN(pass_index >= 0 && pass_index < int(get_internal()->passes.size()));

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
	re_initialize_column_refcounts();
}

// Internal

std::shared_ptr<Internal> VoxelGeneratorMultipassCB::get_internal() const {
	MutexLock mlock(_internal_mutex);
	ZN_ASSERT(_internal != nullptr);
	return _internal;
}

namespace {

int get_total_dependency_extent(const Internal &generator) {
	int extent = 0;
	for (unsigned int pass_index = 1; pass_index < generator.passes.size(); ++pass_index) {
		const Pass &pass = generator.passes[pass_index];
		if (pass.dependency_extents == 0) {
			ZN_PRINT_ERROR("Unexpected pass dependency extents");
		}
		extent += pass.dependency_extents * 2;
	}
	return extent;
}

inline Vector2i to_vec2i_xz(Vector3i p) {
	return Vector2i(p.x, p.z);
}

Box2i to_box2i_in_height_range(Box3i box3, int min_y, int height) {
	if (box3.position.y + box3.size.y <= min_y || box3.position.y >= min_y + height) {
		// Empty box because it doesn't intersect the height range
		return Box2i(to_vec2i_xz(box3.position), Vector2i());
	}
	return Box2i(to_vec2i_xz(box3.position), to_vec2i_xz(box3.size));
}

} // namespace

void VoxelGeneratorMultipassCB::generate_pass(PassInput input) {
	ZN_PROFILE_SCOPE();

	// Note: must not access _internal from here, only use `input`

	if (get_script() != Variant()) {
		// TODO Cache it?
		Ref<VoxelToolMultipassGenerator> vt;
		vt.instantiate();
		vt->set_pass_input(input);

		{
			ZN_GODOT_CHECK_REF_COUNT_DOES_NOT_CHANGE(vt);
			GDVIRTUAL_CALL(_generate_pass, vt, input.pass_index);
		}

		{
			ZN_PROFILE_SCOPE_NAMED("Compress uniform blocks");
			for (Block *block : input.grid) {
				if (block != nullptr) {
					block->voxels.compress_uniform_channels();
				}
			}
		}
	}
}

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

	} else {
		paired_viewer->request_box = p_requested_box;
	}

	process_viewer_diff_internal(p_requested_box, p_prev_requested_box);
}

void VoxelGeneratorMultipassCB::process_viewer_diff_internal(Box3i p_requested_box, Box3i p_prev_requested_box) {
	ZN_DSTACK();
	ZN_PROFILE_SCOPE();
	// TODO Could run as a task similarly to threaded update of VLT
	// However if we do that we need to make sure block requests dont end up cancelled due to no block being found
	// to load... the easiest way I can think of, is to just run this in the same thread that triggers the requests,
	// and that means moving VoxelTerrain's process to a thread as well.

	std::shared_ptr<Internal> internal = get_internal();

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

	// println(format("L {} {} {} {} {}", load_requested_box.pos.x, load_requested_box.pos.y,
	// load_requested_box.size.x, 		load_requested_box.size.y, Time::get_singleton()->get_ticks_usec()));

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
							// (also because we are locking the map, that task must not run until we're done
							// removing its target column)
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

	// We dont reset viewer refcounts, we assume they will be re-paired later by the caller.

	// re_initialize_column_refcounts();

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

bool VoxelGeneratorMultipassCB::debug_try_get_column_states(StdVector<DebugColumnState> &out_states) {
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

#ifdef TOOLS_ENABLED

void VoxelGeneratorMultipassCB::get_configuration_warnings(PackedStringArray &out_warnings) const {
	if (get_script() == Variant()) {
		out_warnings.append(
				String("{0} needs a script implementing `_generate_pass`.").format(varray(get_class_static())));
	}
}

#endif

TypedArray<godot::VoxelBuffer> VoxelGeneratorMultipassCB::debug_generate_test_column(Vector2i column_position_blocks) {
	// struct L {
	// 	static void debug_print_blocks_with_stone(const Column &column, unsigned int column_index) {
	// 		for (unsigned int block_index = 0; block_index < column.blocks.size(); ++block_index) {
	// 			const Block &block = column.blocks[block_index];
	// 			const Vector3i bs = block.voxels.get_size();
	// 			Vector3i pos;
	// 			int stone_count = 0;
	// 			for (pos.z = 0; pos.z < bs.z; ++pos.z) {
	// 				for (pos.x = 0; pos.x < bs.x; ++pos.x) {
	// 					for (pos.y = 0; pos.y < bs.y; ++pos.y) {
	// 						const int v = block.voxels.get_voxel(pos, 0);
	// 						if (v == 1) {
	// 							stone_count++;
	// 						}
	// 					}
	// 				}
	// 			}
	// 			if (stone_count > 0) {
	// 				println(format("Column {} Block {} has {} stone", column_index, block_index, stone_count));
	// 			}
	// 		}
	// 	}

	// 	static void debug_print_blocks_with_stone(const StdVector<Column> &columns) {
	// 		for (unsigned int column_index = 0; column_index < columns.size(); ++column_index) {
	// 			const Column &column = columns[column_index];
	// 			debug_print_blocks_with_stone(column, column_index);
	// 		}
	// 	}
	// };
	ZN_PROFILE_SCOPE();
	// TODO Allow specifying a target pass? Currently this runs up to the final pass

	std::shared_ptr<Internal> internal = get_internal();
	VoxelGeneratorMultipassCB &generator = *this;

	const int total_extent = get_total_dependency_extent(*internal);

	const Vector2i grid_origin = column_position_blocks;

	const Vector2i grid_size = Vector2iUtil::create(total_extent * 2 + 1);

	const int subpass_count = get_subpass_count_from_pass_count(internal->passes.size());
	Box2i local_box(Vector2i(), grid_size);

	StdVector<Column> columns;
	columns.resize(Vector2iUtil::get_area(grid_size));

	for (Column &column : columns) {
		column.blocks.resize(internal->column_height_blocks);
		for (Block &block : column.blocks) {
			block.voxels.create(Vector3iUtil::create(1 << constants::DEFAULT_BLOCK_SIZE_PO2));
		}
	}

	for (int subpass_index = 0; subpass_index < subpass_count; ++subpass_index) {
		const int pass_index = VoxelGeneratorMultipassCB::get_pass_index_from_subpass(subpass_index);
		const int prev_pass_index = VoxelGeneratorMultipassCB::get_pass_index_from_subpass(subpass_index - 1);
		const Pass &pass = internal->passes[pass_index];
		const int extent = pass.dependency_extents;

		local_box.for_each_cell_yx([extent, &columns, grid_size, grid_origin, internal, pass_index, prev_pass_index,
										   &generator](Vector2i local_bpos) {
			if (pass_index == 0 || pass_index != prev_pass_index) {
				const Box2i nbox = Box2i(local_bpos, Vector2i(1, 1)).padded(extent);

				StdVector<Block *> ngrid;
				ngrid.reserve(Vector2iUtil::get_area(nbox.size));

				// Compose grid of blocks indexed as ZXY (index+1 goes up along Y).
				// ZXY indexing is convenient here, since columns are indexed with YX (aka ZX, because Y in 2D is Z
				// in 3D)
				nbox.for_each_cell_yx([&ngrid, &columns, grid_size](Vector2i cpos) {
					const int src_loc = Vector2iUtil::get_yx_index(cpos, grid_size);
					Column &column = columns[src_loc];
					for (Block &block : column.blocks) {
						ngrid.push_back(&block);
					}
				});

				const Vector2i grid_origin_2d = grid_origin + nbox.position;
				const Vector2i main_origin_2d =
						grid_origin_2d + Vector2iUtil::create(extent); // grid_origin + local_bpos

				PassInput pass_input;
				pass_input.grid = to_span(ngrid);
				pass_input.grid_origin = Vector3i(grid_origin_2d.x, internal->column_base_y_blocks, grid_origin_2d.y);
				pass_input.grid_size = Vector3i(nbox.size.x, internal->column_height_blocks, nbox.size.y);
				pass_input.pass_index = pass_index;
				pass_input.main_block_position = Vector3i(main_origin_2d.x, pass_input.grid_origin.y, main_origin_2d.y);
				pass_input.block_size = 1 << constants::DEFAULT_BLOCK_SIZE_PO2;
				generator.generate_pass(pass_input);

				// if (pass_index == 1) {
				// 	L::debug_print_blocks_with_stone(columns);
				// }
			}

			// Skipping control fields on Column since we are doing this single-threaded in isolation. However if
			// one day we migrate this to work directly on the cache, we will have to update them (that also means
			// it will have race conditions)
		});

		const int next_pass_index = get_pass_index_from_subpass(subpass_index + 1);
		const Pass &next_pass = internal->passes[next_pass_index];
		local_box = local_box.padded(-next_pass.dependency_extents);
	}

	const Vector2i final_column_rpos = Vector2iUtil::create(total_extent);
	const int final_column_loc = Vector2iUtil::get_yx_index(final_column_rpos, grid_size);
	Column &final_column = columns[final_column_loc];
	// L::debug_print_blocks_with_stone(final_column, final_column_loc);

	// Wrap up result for script API
	TypedArray<godot::VoxelBuffer> column_ta;
	column_ta.resize(final_column.blocks.size());
	for (unsigned int i = 0; i < final_column.blocks.size(); ++i) {
		Block &block = final_column.blocks[i];
		Ref<godot::VoxelBuffer> buffer;
		buffer.instantiate();
		block.voxels.move_to(buffer->get_buffer());
		column_ta[i] = buffer;
	}

	return column_ta;
}

// BINDING LAND

bool VoxelGeneratorMultipassCB::_set(const StringName &p_name, const Variant &p_value) {
	const String property_name = p_name;

	if (property_name.begins_with("pass_")) {
		const int index_pos = string_literal_length("pass_");
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
		const int index_pos = string_literal_length("pass_");
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

	ClassDB::bind_method(D_METHOD("debug_generate_test_column", "column_position_blocks"),
			&VoxelGeneratorMultipassCB::debug_generate_test_column);

#if defined(ZN_GODOT)
	// TODO Test if GDVIRTUAL can print errors properly when GDScript fails inside a different thread.
	GDVIRTUAL_BIND(_generate_pass, "voxel_tool", "pass_index");
	GDVIRTUAL_BIND(_generate_block_fallback, "out_buffer", "origin_in_voxels");
	GDVIRTUAL_BIND(_get_used_channels_mask);
#endif

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
