#include "generate_block_multipass_task.h"
#include "../../engine/voxel_engine.h"
#include "../../util/profiling.h"

#include "../../util/godot/classes/time.h"
#include "../../util/string_funcs.h"

namespace zylann::voxel {

namespace {
std::atomic_int g_task_count[VoxelGeneratorMultipass::MAX_SUBPASSES] = { 0 };
const char *g_profiling_task_names[VoxelGeneratorMultipass::MAX_SUBPASSES] = {
	"GenerateBlockMultipassTasks_subpass0",
	"GenerateBlockMultipassTasks_subpass1",
	"GenerateBlockMultipassTasks_subpass2",
	"GenerateBlockMultipassTasks_subpass3",
	"GenerateBlockMultipassTasks_subpass4",
	"GenerateBlockMultipassTasks_subpass5",
	"GenerateBlockMultipassTasks_subpass6",
};

} // namespace

GenerateBlockMultipassTask::GenerateBlockMultipassTask(Vector3i p_block_position, uint8_t p_block_size,
		uint8_t p_subpass_index, Ref<VoxelGeneratorMultipass> p_generator, IThreadedTask *p_caller,
		std::shared_ptr<std::atomic_int> p_caller_dependency_count) {
	//
	_block_position = p_block_position;
	_block_size = p_block_size;
	_subpass_index = p_subpass_index;
	_generator = p_generator;
	_caller_task = p_caller;
	ZN_ASSERT(p_caller_dependency_count != nullptr);
	ZN_ASSERT(*p_caller_dependency_count > 0);
	_caller_task_dependency_counter = p_caller_dependency_count;

	int64_t v = ++g_task_count[_subpass_index];
	ZN_PROFILE_PLOT(g_profiling_task_names[_subpass_index], v);
}

GenerateBlockMultipassTask::~GenerateBlockMultipassTask() {
	// Don't loose the caller task, it needs to be resumed or properly cancelled.
	ZN_ASSERT(_caller_task == nullptr);

	if (_caller_task_dependency_counter.use_count() == 1) {
		ZN_ASSERT(*_caller_task_dependency_counter == 0);
	}

	int64_t v = --g_task_count[_subpass_index];
	ZN_PROFILE_PLOT(g_profiling_task_names[_subpass_index], v);
}

void GenerateBlockMultipassTask::run(ThreadedTaskContext &ctx) {
	ZN_PROFILE_SCOPE();
	ZN_ASSERT(_generator.is_valid());

	std::shared_ptr<VoxelGeneratorMultipass::Map> map = _generator->get_map();

	const int pass_index_ = VoxelGeneratorMultipass::get_pass_index_from_subpass(_subpass_index);
	const VoxelGeneratorMultipass::Pass &pass = _generator->get_pass(pass_index_);

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
	blocks.reserve(Vector3iUtil::get_volume(neighbors_box.size));

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
				std::shared_ptr<VoxelGeneratorMultipass::Block> &block = map->blocks[bpos];
				if (block == nullptr) {
					block = make_shared_instance<VoxelGeneratorMultipass::Block>();
				}
				blocks.push_back(block);
			});
		}

		struct BlockToGenerate {
			Vector3i position;
			std::shared_ptr<VoxelGeneratorMultipass::Block> block;
		};
		std::vector<BlockToGenerate> to_generate;

		const int subpass_index = _subpass_index;
		const int prev_subpass_index = subpass_index - 1;

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
						ZN_ASSERT(block != nullptr);

						// We want all blocks in the neighborhood to be at least at the previous subpass before we can
						// run the current subpass
						if (block->subpass_index < prev_subpass_index) {
							to_generate.push_back({ bpos, block });
						}

						++i;
					}
				}
			}
		}

		BufferedTaskScheduler &scheduler = BufferedTaskScheduler::get_for_current_thread();

		if (to_generate.size() > 0) {
			ZN_ASSERT(_subpass_index > 0);

			// If we come back here after having requested dependencies (and we never postpone), then it means
			// tasks we spawned or were waiting for have not fulfilled our expectations, so something must be wrong
			ZN_ASSERT(!_debug_has_requested_deps);

			ZN_PROFILE_SCOPE_NAMED("Spawn subtasks");
			// Generate dependencies

			std::shared_ptr<std::atomic_int> counter = make_shared_instance<std::atomic_int>(to_generate.size());

			for (const BlockToGenerate &todo : to_generate) {
				GenerateBlockMultipassTask *task = ZN_NEW(GenerateBlockMultipassTask(
						todo.position, _block_size, prev_subpass_index, _generator, this, counter));
				scheduler.push_main_task(task);
			}

			// We queued the current task after the tasks we spawned
			scheduler.flush();
			ctx.status = ThreadedTaskContext::STATUS_TAKEN_OUT;

			_debug_has_requested_deps = true;

			// TODO Can we keep a reference to blocks we just fetched so we avoid fetching them again next time?
			// It sounds like we can, however those blocks could get unloaded in the meantime...

			return;

		} else {
			ZN_PROFILE_SCOPE_NAMED("Run pass");
			// We can run the pass

			std::shared_ptr<VoxelGeneratorMultipass::Block> main_block = blocks[central_block_index];

			if (main_block->subpass_index == prev_subpass_index) {
				if (_subpass_index == 0) {
					// First pass creates the block
					for (std::shared_ptr<VoxelGeneratorMultipass::Block> block : blocks) {
						block->voxels.create(Vector3iUtil::create(_block_size));
					}
				}

				// Debug check
				// const int subpass_completion_iterations = math::cubed(pass.dependency_extents * 2 + 1);
				// ZN_ASSERT(main_block->subpass_iterations[_subpass_index] < subpass_completion_iterations);

				VoxelGeneratorMultipass::PassInput input;
				input.grid = to_span(blocks);
				input.grid_size = neighbors_box.size;
				input.grid_origin = neighbors_box.pos;
				input.main_block_position = _block_position;
				input.pass_index = pass_index_;

				_generator->generate_pass(input);

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

			// if (_full) {
			// 	main_block->pending_full_generation_tasks &= ~(1 << pass_index);
			// } else {
			// 	main_block->pending_partial_generation_tasks &= ~(1 << pass_index);
			// }
		}
	} // Region lock

	return_to_caller();
}

// TODO Implement priority
// TaskPriority get_priority() {}

// TODO Implement cancellation
// bool is_cancelled() {}

void GenerateBlockMultipassTask::return_to_caller() {
	// No use case not using this
	ZN_ASSERT(_caller_task != nullptr);

	if (_caller_task != nullptr) {
		ZN_ASSERT(_caller_task_dependency_counter != nullptr);
		const int counter = --(*_caller_task_dependency_counter);
		ZN_ASSERT(counter >= 0);
		if (counter == 0) {
			VoxelEngine::get_singleton().push_async_task(_caller_task);
		}
		_caller_task = nullptr;
	}
}

} // namespace zylann::voxel
