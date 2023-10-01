#include "generate_block_multipass_pm_task.h"
#include "../../engine/voxel_engine.h"
#include "../../storage/voxel_data.h"

#include "../../util/godot/classes/time.h"
#include "../../util/string_funcs.h"

namespace zylann::voxel {

namespace {
std::atomic_int g_task_count[VoxelGeneratorMultipass::MAX_SUBPASSES] = { 0 };
const char *g_profiling_task_names[VoxelGeneratorMultipass::MAX_SUBPASSES] = {
	"GenerateBlockMultipassPMTasks_subpass0",
	"GenerateBlockMultipassPMTasks_subpass1",
	"GenerateBlockMultipassPMTasks_subpass2",
	"GenerateBlockMultipassPMTasks_subpass3",
	"GenerateBlockMultipassPMTasks_subpass4",
	"GenerateBlockMultipassPMTasks_subpass5",
	"GenerateBlockMultipassPMTasks_subpass6",
};

} // namespace

GenerateBlockMultipassPMTask::GenerateBlockMultipassPMTask(Vector3i p_block_position, uint8_t p_block_size,
		uint8_t p_subpass_index, Ref<VoxelGeneratorMultipass> p_generator, TaskPriority p_priority) {
	//
	_block_position = p_block_position;
	_block_size = p_block_size;
	_subpass_index = p_subpass_index;

	ZN_ASSERT(p_generator.is_valid());
	_generator = p_generator;

	_priority = p_priority;

	int64_t v = ++g_task_count[_subpass_index];
	ZN_PROFILE_PLOT(g_profiling_task_names[_subpass_index], v);
}

GenerateBlockMultipassPMTask::~GenerateBlockMultipassPMTask() {
	int64_t v = --g_task_count[_subpass_index];
	ZN_PROFILE_PLOT(g_profiling_task_names[_subpass_index], v);
}

void GenerateBlockMultipassPMTask::run(ThreadedTaskContext &ctx) {
	ZN_PROFILE_SCOPE();
	ZN_ASSERT(_generator.is_valid());

	const int pass_index = VoxelGeneratorMultipass::get_pass_index_from_subpass(_subpass_index);
	const VoxelGeneratorMultipass::Pass &pass = _generator->get_pass(pass_index);

	const int final_subpass_index =
			VoxelGeneratorMultipass::get_subpass_count_from_pass_count(_generator->get_pass_count()) - 1;

	if (_subpass_index == 0) {
		// The first subpass can't depend on another subpass
		ZN_ASSERT(pass.dependency_extents == 0);
	} else {
		ZN_ASSERT(pass.dependency_extents > 0);
	}

	// TODO Handle column-based passes

	const Box3i neighbors_box = Box3i::from_min_max( //
			_block_position - Vector3iUtil::create(pass.dependency_extents),
			_block_position + Vector3iUtil::create(pass.dependency_extents + 1));

	const unsigned int central_block_index =
			Vector3iUtil::get_zxy_index(Vector3iUtil::create(pass.dependency_extents), neighbors_box.size);

	// Grid of blocks we'll be working with
	std::vector<std::shared_ptr<VoxelGeneratorMultipass::Block>> blocks;
	// TODO Cache memory
	blocks.reserve(Vector3iUtil::get_volume(neighbors_box.size));

	std::shared_ptr<VoxelGeneratorMultipass::Map> map = _generator->get_map();

	IThreadedTask *next_task = nullptr;

	// Lock region we are going to process
	{
		ZN_PROFILE_SCOPE_NAMED("Region");

		// Blocking until available causes bottlenecks. Not always big ones, but enough to be very noticeable in the
		// profiler.
		// VoxelSpatialLockWrite swlock(map->spatial_lock, neighbors_box);
		if (!map->spatial_lock.try_lock_write(neighbors_box)) {
			// Try later
			ctx.status = ThreadedTaskContext::STATUS_POSTPONED;
			return;
		}
		// Sometimes I wish `defer` was a thing in C++
		VoxelSpatialLock_UnlockWriteOnScopeExit swlock(map->spatial_lock, neighbors_box);

		// Fetch blocks from map
		{
			ZN_PROFILE_SCOPE_NAMED("Fetch blocks");

			// Locking for writing because we can create new blocks.
			// Profiling hasn't shown significant bottleneck here. If it happens though, we can use postponing.
			MutexLock mlock(map->mutex);

			// TODO Handle world bounds
			Vector3i bpos;
			neighbors_box.for_each_cell_zxy([&blocks, &map](Vector3i bpos) {
				auto it = map->blocks.find(bpos);
				std::shared_ptr<VoxelGeneratorMultipass::Block> block = nullptr;
				if (it != map->blocks.end()) {
					block = it->second;
				}
				blocks.push_back(block);
			});
		}

		const int subpass_index = _subpass_index;
		const int prev_subpass_index = subpass_index - 1;

		std::shared_ptr<VoxelGeneratorMultipass::Block> main_block = blocks[central_block_index];

		// Check loading levels
		{
			ZN_PROFILE_SCOPE_NAMED("Check levels");

			Vector3i bpos;
			unsigned int i = 0;

			const Vector3i bpos_min = neighbors_box.pos;
			const Vector3i bpos_max = neighbors_box.pos + neighbors_box.size;

			for (bpos.z = bpos_min.z; bpos.z < bpos_max.z; ++bpos.z) {
				for (bpos.x = bpos_min.x; bpos.x < bpos_max.x; ++bpos.x) {
					for (bpos.y = bpos_min.y; bpos.y < bpos_max.y; ++bpos.y) {
						std::shared_ptr<VoxelGeneratorMultipass::Block> block = blocks[i];
						if (block == nullptr) {
							// No longer loaded, we have to cancel the task

							if (main_block != nullptr) {
								main_block->pending_subpass_tasks_mask &= ~(1 << _subpass_index);
							}

							return;
						}

						// [Code not using subpasses, trying to rely on some kind of refcount]
						// if (pass_index > 0 && block->pass_iterations[prev_pass_index] <
						// expected_prev_pass_iterations) {
						// 	// Dependencies not ready yet.
						// 	// TODO But how can we tell if the neighbor has neighbors itself with tasks pending to
						// 	// complete the dependency? Yet another nightmare checking neighbors...
						// }

						// We want all blocks in the neighborhood to be at least at the previous subpass before we can
						// run the current subpass
						if (prev_subpass_index >= 0 && block->subpass_index < prev_subpass_index) {
							// Dependencies not ready yet.

							if (block->loading ||
									(block->pending_subpass_tasks_mask & (1 << prev_subpass_index)) != 0) {
								// A task is pending to work on the dependency, so we wait.
								ctx.status = ThreadedTaskContext::STATUS_POSTPONED;
								return;

							} else {
								// No task is pending to work on the dependency, we have to cancel.
								if (main_block != nullptr) {
									main_block->pending_subpass_tasks_mask &= ~(1 << _subpass_index);
								}
								return;
							}
						}

						++i;
					}
				}
			}
		}

		{
			ZN_PROFILE_SCOPE_NAMED("Run pass");
			// We can run the pass

			ZN_ASSERT(main_block != nullptr);

			if (main_block->subpass_index == prev_subpass_index) {
				if (_subpass_index == 0) {
					// First pass creates the block
					main_block->voxels.create(Vector3iUtil::create(_block_size));
				}

				// Debug check
				// const int subpass_completion_iterations = math::cubed(pass.dependency_extents * 2 + 1);
				// ZN_ASSERT(main_block->subpass_iterations[_subpass_index] < subpass_completion_iterations);

				// The fact we split passes in two doesn't mean we should run the generator again each time. Instead, we
				// may run the generator only on the first subpass referring to a given pass.
				// TODO that means part of the tasks we spawn don't actually do expensive work. Could we simplify this
				// further?
				const int prev_pass_index = VoxelGeneratorMultipass::get_pass_index_from_subpass(prev_subpass_index);

				if (pass_index == 0 || prev_pass_index != pass_index) {
					VoxelGeneratorMultipass::PassInput input;
					input.grid = to_span(blocks);
					input.grid_size = neighbors_box.size;
					input.grid_origin = neighbors_box.pos;
					input.main_block_position = _block_position;
					input.pass_index = pass_index;

					_generator->generate_pass(input);
				}

				// Update levels
				main_block->subpass_index = _subpass_index;

				for (std::shared_ptr<VoxelGeneratorMultipass::Block> block : blocks) {
					block->subpass_iterations[_subpass_index]++;
				}

				// Pass, X, Y, Z, Time
				// println(format("P {} {} {} {} {}", int(_subpass_index), _block_position.x, _block_position.y,
				// 		_block_position.z, Time::get_singleton()->get_ticks_usec()));

			} else if (main_block->subpass_index == subpass_index) {
				// Already processed. Might have been that this block was in partial state before and just needed
				// neighbors to be updated?

			} else {
				// Can happen more rarely because multithreading. More than one task can end up
				// processing the chunk of the current task, due to the pattern of spawning sub-tasks. The current task
				// can be queued for a while and when its subtasks finish, stuff will have happened since. Not easy to
				// predict task trees when every chunk depends on their neighbors.
				// ZN_PRINT_VERBOSE("WTF");
			}

			main_block->pending_subpass_tasks_mask &= ~(1 << _subpass_index);
		}

		if (main_block->subpass_index == final_subpass_index && main_block->final_pending_task != nullptr) {
			next_task = main_block->final_pending_task;
			main_block->final_pending_task = nullptr;
		}

	} // Region lock

	if (next_task != nullptr) {
		VoxelEngine::get_singleton().push_async_task(next_task);
	}
}

// TODO Implement priority
// TaskPriority get_priority() {}

// TODO Implement cancellation
// bool is_cancelled() {}

} // namespace zylann::voxel
