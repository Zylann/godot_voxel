#include "voxel_generator_multipass.h"
#include "../../engine/voxel_engine.h"
#include "../../util/profiling.h"
#include "../../util/string_funcs.h"
#include "generate_block_multipass_pm_task.h"

#include "../../util/noise/fast_noise_lite/fast_noise_lite.h"

namespace zylann::voxel {

VoxelGeneratorMultipass::VoxelGeneratorMultipass() {
	_map = make_shared_instance<VoxelGeneratorMultipass::Map>();

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

VoxelGeneratorMultipass::~VoxelGeneratorMultipass() {}

VoxelGeneratorMultipass::Map::~Map() {
	// If the map gets destroyed then we know the last reference to it was removed, which means only one thread had
	// access to it, so we can get away not locking anything.

	// Initially I thought this is where we'd re-schedule any task pointers still in blocks, but that can't happen,
	// because if tasks exist referencing the multipass generator, then the generator (and so the map) would not be
	// destroyed, and so we wouldn't be here.
	// Therefore there is only just an assert in ~Block to ensure the only other case where blocks are removed from the
	// map before handling these tasks, or to warn us in case there is a case we didn't expect.
}

VoxelGenerator::Result VoxelGeneratorMultipass::generate_block(VoxelQueryData &input) {
	// TODO Fallback on an expensive single-threaded dependency generation, which we might throw away after
	ZN_PRINT_ERROR("Not implemented");
	return { false };
}

namespace {
// TODO Placeholder utility to emulate what would be the final use case
struct GridEditor {
	Span<std::shared_ptr<VoxelGeneratorMultipass::Block>> blocks;
	Box3i world_box;

	inline std::shared_ptr<VoxelGeneratorMultipass::Block> get_block_and_relative_position(
			Vector3i voxel_pos_world, Vector3i &voxel_pos_block) {
		const Vector3i block_pos_world = voxel_pos_world >> 4;
		ZN_ASSERT(world_box.contains(block_pos_world));
		const Vector3i block_pos_grid = block_pos_world - world_box.pos;
		const unsigned int grid_loc = Vector3iUtil::get_zxy_index(block_pos_grid, world_box.size);
		std::shared_ptr<VoxelGeneratorMultipass::Block> block = blocks[grid_loc];
		ZN_ASSERT(block != nullptr);
		voxel_pos_block = voxel_pos_world & 15;
		return block;
	}

	int get_voxel(Vector3i voxel_pos_world, int value, int channel) {
		Vector3i voxel_pos_block;
		std::shared_ptr<VoxelGeneratorMultipass::Block> block =
				get_block_and_relative_position(voxel_pos_world, voxel_pos_block);
		return block->voxels.get_voxel(voxel_pos_block, channel);
	}

	void set_voxel(Vector3i voxel_pos_world, int value, int channel) {
		Vector3i voxel_pos_block;
		std::shared_ptr<VoxelGeneratorMultipass::Block> block =
				get_block_and_relative_position(voxel_pos_world, voxel_pos_block);
		block->voxels.set_voxel(value, voxel_pos_block, channel);
	}
};

int get_total_dependency_extent(const VoxelGeneratorMultipass &generator) {
	int extent = 1;
	for (int pass_index = 1; pass_index < generator.get_pass_count(); ++pass_index) {
		const VoxelGeneratorMultipass::Pass &pass = generator.get_pass(pass_index);
		if (pass.dependency_extents == 0) {
			ZN_PRINT_ERROR("Unexpected pass dependency extents");
		}
		extent += pass.dependency_extents * 2;
	}
	return extent;
}

} // namespace

void VoxelGeneratorMultipass::generate_pass(PassInput input) {
	// PLACEHOLDER
	const Vector3i origin_in_voxels = input.main_block_position * 16;
	static const VoxelBufferInternal::ChannelId channel = VoxelBufferInternal::CHANNEL_TYPE;

	if (input.pass_index == 0) {
		Ref<ZN_FastNoiseLite> fnl;
		fnl.instantiate();
		// Pass 0 has no neighbor access so we have just one block
		std::shared_ptr<Block> block = input.grid[0];
		ZN_ASSERT(block != nullptr);
		VoxelBufferInternal &voxels = block->voxels;
		Vector3i rpos;
		const Vector3i bs = voxels.get_size();

		for (rpos.z = 0; rpos.z < bs.z; ++rpos.z) {
			for (rpos.x = 0; rpos.x < bs.x; ++rpos.x) {
				for (rpos.y = 0; rpos.y < bs.y; ++rpos.y) {
					Vector3i pos = origin_in_voxels + rpos;
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

	} else if (input.pass_index == 1) {
		GridEditor editor;
		editor.blocks = input.grid;
		editor.world_box = Box3i(input.grid_origin, input.grid_size);

		const uint32_t h = Variant(input.grid_origin).hash();

		Vector3i structure_center;
		structure_center.x = h & 15;
		structure_center.y = (h >> 4) & 15;
		structure_center.z = (h >> 8) & 15;
		const int structure_size = (h >> 12) & 15;

		const Box3i structure_box(structure_center - Vector3iUtil::create(structure_size / 2) + origin_in_voxels,
				Vector3iUtil::create(structure_size));

		structure_box.for_each_cell_zxy([&editor](Vector3i pos) { //
			editor.set_voxel(pos, 1, channel);
		});

	} else if (input.pass_index == 2) {
		// blep
		Thread::sleep_usec(1000);
	}
}

void VoxelGeneratorMultipass::process_viewer_diff(Box3i p_requested_box, Box3i p_prev_requested_box) {
	ZN_PROFILE_SCOPE();
	// TODO Could run as a task similarly to threaded update of VLT
	// However if we do that we need to make sure block requests dont end up cancelled due to no block being found to
	// load... the easiest way I can think of, is to just run this in the same thread that triggers the requests, and
	// that means moving VoxelTerrain's process to a thread as well.

	static const int block_size = 1 << constants::DEFAULT_BLOCK_SIZE_PO2;

	const int total_extent = get_total_dependency_extent(*this);

	// Note: empty boxes should not be padded, they mean nothing is requested, so the padded request must also be empty.
	const Box3i load_requested_box =
			p_requested_box.is_empty() ? p_requested_box : p_requested_box.padded(total_extent);
	const Box3i prev_load_requested_box =
			p_prev_requested_box.is_empty() ? p_prev_requested_box : p_prev_requested_box.padded(total_extent);

	Map &map = *_map;

	BufferedTaskScheduler &task_scheduler = BufferedTaskScheduler::get_for_current_thread();

	// Blocks to view
	load_requested_box.difference(prev_load_requested_box, [&map](Box3i new_box) {
		{
			VoxelSpatialLockWrite swlock(map.spatial_lock, new_box);
			MutexLock mlock(map.mutex);

			new_box.for_each_cell_zxy([&map](Vector3i bpos) {
				std::shared_ptr<Block> &block = map.blocks[bpos];
				if (block == nullptr) {
					block = make_unique_instance<Block>();
					// block->loading = true;
				}
				block->viewers.add();
			});

			// TODO Implement loading tasks
		}
	});

	// What happens if blocks are unloaded, then requested again before the save task has finished?
	// We could have an unloading state, at the end of which the block gets removed once it got saved.
	// But then what should the request do when it finds the unloading block?
	// Cancel the unloading state. Since that means a saving task is underway, the saving task should check that state
	// again before removing the block. If the state is no longer active, the task will simply not remove the block
	// (acting like a regular save). The "unloading" state could be seen as the refcount being 0. If the request is
	// late, it will find no block, triggering loading, which should work as usual.

	// Blocks to unview
	prev_load_requested_box.difference(load_requested_box, [&map, &task_scheduler](Box3i old_box) {
		{
			VoxelSpatialLockWrite swlock(map.spatial_lock, old_box);
			MutexLock mlock(map.mutex);

			old_box.for_each_cell_zxy([&map, &task_scheduler](Vector3i bpos) {
				auto it = map.blocks.find(bpos);

				// The block must be found because last time the block was in the loading area of the viewer.
				ZN_ASSERT(it != map.blocks.end());
				std::shared_ptr<Block> block = it->second;
				ZN_ASSERT(block != nullptr);

				block->viewers.remove();
				if (block->viewers.get() == 0) {
					if (block->final_pending_task != nullptr) {
						// There was a pending generate task, resume it, but it will basically return a drop.
						task_scheduler.push_main_task(block->final_pending_task);
						block->final_pending_task = nullptr;
					}

					// TODO Implement saving tasks
					// We remove immediately for now
					map.blocks.erase(it);
				}
			});
		}
	});

	const int subpass_count = get_subpass_count_from_pass_count(get_pass_count());

	Box3i subpass_box = load_requested_box;
	Box3i prev_subpass_box = prev_load_requested_box;
	Ref<VoxelGeneratorMultipass> generator_ref(this);

	const Vector3i viewer_block_pos = p_requested_box.pos + p_requested_box.size / 2;

	// Pass generation tasks
	for (int subpass_index = 0; subpass_index < subpass_count; ++subpass_index) {
		// Debug checks: box shrinking must not make boxes empty
		if (subpass_index > 0) {
			// Shrink boxes for next subpass
			const int next_pass_index = VoxelGeneratorMultipass::get_pass_index_from_subpass(subpass_index);
			const VoxelGeneratorMultipass::Pass &next_pass = get_pass(next_pass_index);
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
				[&map, subpass_index, &task_scheduler, &generator_ref, viewer_block_pos](Box3i new_box) {
					VoxelSpatialLockWrite swlock(map.spatial_lock, new_box);
					MutexLock mlock(map.mutex);

					new_box.for_each_cell_zxy([&map, subpass_index, &task_scheduler, &generator_ref, viewer_block_pos](
													  Vector3i bpos) {
						auto it = map.blocks.find(bpos);

						// The block must be found since we are inside the loading area of the viewer.
						ZN_ASSERT(it != map.blocks.end());
						std::shared_ptr<Block> block = it->second;
						ZN_ASSERT(block != nullptr);

						const unsigned int subpass_bit = (1 << subpass_index);

						if (block->subpass_index < subpass_index &&
								(block->pending_subpass_tasks_mask & subpass_bit) == 0) {
							block->pending_subpass_tasks_mask |= subpass_bit;

							const int distance = math::chebyshev_distance(viewer_block_pos, bpos);

							TaskPriority priority;
							// Priority decreases over distance.
							// Later subpasses also have lower priority since they depend on first passes.
							priority.band0 = math::max(TaskPriority::BAND_MAX - distance - 2 * subpass_index, 0);
							priority.band2 = constants::TASK_PRIORITY_LOAD_BAND2;

							// TODO Consider allocating tasks outside the locking scope
							GenerateBlockMultipassPMTask *task = ZN_NEW(GenerateBlockMultipassPMTask(
									bpos, block_size, subpass_index, generator_ref, priority));

							task_scheduler.push_main_task(task);
						}
					});
				});
	}

	task_scheduler.flush();
}

} // namespace zylann::voxel
