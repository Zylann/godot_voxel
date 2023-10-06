#include "voxel_generator_multipass_cb.h"
#include "../../engine/voxel_engine.h"
#include "../../util/profiling.h"
#include "../../util/string_funcs.h"
#include "generate_block_multipass_pm_cb_task.h"

#include "../../util/dstack.h"
#include "../../util/godot/classes/time.h"
#include "../../util/noise/fast_noise_lite/fast_noise_lite.h"

namespace zylann::voxel {

VoxelGeneratorMultipassCB::VoxelGeneratorMultipassCB() {
	_map = make_shared_instance<VoxelGeneratorMultipassCB::Map>();

	// PLACEHOLDER
	{
		Pass pass;
		_passes.push_back(pass);
	}
	{
		Pass pass;
		pass.dependency_extents = 1;
		_passes.push_back(pass);
	}
	{
		Pass pass;
		pass.dependency_extents = 1;
		_passes.push_back(pass);
	}
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

	if (input.origin_in_voxels.y + bs <= _column_base_y_blocks * bs) {
		// TODO Generate bottom fallback

	} else if (input.origin_in_voxels.y >= (_column_base_y_blocks + _column_height_blocks) * bs) {
		// TODO Generate top fallback

	} else {
		// Can't generate column chunks from here for now
		// TODO Fallback on an expensive single-threaded dependency generation, which we might throw away after
		ZN_PRINT_ERROR("Not implemented");
	}

	return { false };
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

int get_total_dependency_extent(const VoxelGeneratorMultipassCB &generator) {
	int extent = 0;
	for (int pass_index = 1; pass_index < generator.get_pass_count(); ++pass_index) {
		const VoxelGeneratorMultipassCB::Pass &pass = generator.get_pass(pass_index);
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

void VoxelGeneratorMultipassCB::process_viewer_diff(Box3i p_requested_box, Box3i p_prev_requested_box) {
	ZN_DSTACK();
	ZN_PROFILE_SCOPE();
	// TODO Could run as a task similarly to threaded update of VLT
	// However if we do that we need to make sure block requests dont end up cancelled due to no block being found to
	// load... the easiest way I can think of, is to just run this in the same thread that triggers the requests, and
	// that means moving VoxelTerrain's process to a thread as well.

	static const int block_size = 1 << constants::DEFAULT_BLOCK_SIZE_PO2;

	const int total_extent = get_total_dependency_extent(*this);

	const Box2i requested_box_2d =
			to_box2i_in_height_range(p_requested_box, _column_base_y_blocks, _column_height_blocks);
	const Box2i prev_requested_box_2d =
			to_box2i_in_height_range(p_prev_requested_box, _column_base_y_blocks, _column_height_blocks);

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

	Map &map = *_map;

	BufferedTaskScheduler &task_scheduler = BufferedTaskScheduler::get_for_current_thread();

	// Blocks to view
	const int column_height = _column_height_blocks;
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

#if 0
	const int subpass_count = get_subpass_count_from_pass_count(get_pass_count());

	Box2i subpass_box = load_requested_box;
	Box2i prev_subpass_box = prev_load_requested_box;
	Ref<VoxelGeneratorMultipassCB> generator_ref(this);

	const Vector2i viewer_block_pos = requested_box_2d.pos + requested_box_2d.size / 2;

	// Pass generation tasks
	for (int subpass_index = 0; subpass_index < subpass_count; ++subpass_index) {
		// Debug checks: box shrinking must not make boxes empty
		if (subpass_index > 0) {
			// Shrink boxes for next subpass
			const int next_pass_index = VoxelGeneratorMultipassCB::get_pass_index_from_subpass(subpass_index);
			const VoxelGeneratorMultipassCB::Pass &next_pass = get_pass(next_pass_index);
			subpass_box = subpass_box.padded(-next_pass.dependency_extents);
			prev_subpass_box = prev_subpass_box.padded(-next_pass.dependency_extents);

			if (!load_requested_box.is_empty()) {
				ZN_ASSERT(!subpass_box.is_empty());
			}
			if (!prev_load_requested_box.is_empty()) {
				ZN_ASSERT(!prev_subpass_box.is_empty());
			}
		}

		// Run pass on new areas
		subpass_box.difference(prev_subpass_box,
				[&map, subpass_index, &task_scheduler, &generator_ref, viewer_block_pos](Box2i new_box) {
					SpatialLock2D::Write swlock(map.spatial_lock, new_box);
					MutexLock mlock(map.mutex);

					new_box.for_each_cell_yx([&map, subpass_index, &task_scheduler, &generator_ref, viewer_block_pos](
													 Vector2i cpos) {
						auto it = map.columns.find(cpos);

						// The block must be found since we are inside the loading area of the viewer.
						ZN_ASSERT(it != map.columns.end());
						Column &column = it->second;

						const unsigned int subpass_bit = (1 << subpass_index);

						if (column.subpass_index < subpass_index &&
								(column.pending_subpass_tasks_mask & subpass_bit) == 0) {
							column.pending_subpass_tasks_mask |= subpass_bit;

							const int distance = math::chebyshev_distance(viewer_block_pos, cpos);

							TaskPriority priority;
							// Priority decreases over distance.
							// Later subpasses also have lower priority since they depend on first passes.
							priority.band0 = math::max(TaskPriority::BAND_MAX - distance - 2 * subpass_index, 0);
							priority.band2 = constants::TASK_PRIORITY_LOAD_BAND2;

							// TODO Consider allocating tasks outside the locking scope
							GenerateBlockMultipassPMCBTask *task = ZN_NEW(GenerateBlockMultipassPMCBTask(
									cpos, block_size, subpass_index, generator_ref, priority));

							// println(format("S {} {} {} {}", subpass_index, cpos.x, cpos.y,
							// 		Time::get_singleton()->get_ticks_usec()));
							task_scheduler.push_main_task(task);
						}
					});
				});
	}

#endif

	task_scheduler.flush();
}


bool VoxelGeneratorMultipassCB::debug_try_get_column_states(std::vector<DebugColumnState> &out_states) {
	ZN_PROFILE_SCOPE();

	out_states.clear();

	{
		unsigned int size = 0;
		{
			MutexLock mlock(_map->mutex);
			size = _map->columns.size();
		}
		out_states.reserve(size);
	}

	if (!_map->spatial_lock.try_lock_read(BoxBounds2i::from_everywhere())) {
		// Don't hang here on the main thread, while generating it's very likely the map is locked somewhere
		return false;
	}
	SpatialLock2D::UnlockReadOnScopeExit srlock(_map->spatial_lock, BoxBounds2i::from_everywhere());

	MutexLock mlock(_map->mutex);

	for (auto it = _map->columns.begin(); it != _map->columns.end(); ++it) {
		Column &column = it->second;
		out_states.push_back(DebugColumnState{ it->first, column.subpass_index });
	}

	return true;
}

} // namespace zylann::voxel
