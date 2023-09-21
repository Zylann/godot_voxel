#include "generate_block_multipass_task.h"
#include "../../engine/voxel_engine.h"
#include "../../util/profiling.h"

#include "../../util/godot/classes/time.h"
#include "../../util/string_funcs.h"

namespace zylann::voxel {

namespace {
std::atomic_int g_task_full_count[VoxelGeneratorMultipass::MAX_PASSES] = { 0 };
std::atomic_int g_task_partial_count[VoxelGeneratorMultipass::MAX_PASSES] = { 0 };
const char *g_profiling_task_names_partial[VoxelGeneratorMultipass::MAX_PASSES] = {
	"GenerateBlockMultipassTasks_partial_pass0",
	"GenerateBlockMultipassTasks_partial_pass1",
	"GenerateBlockMultipassTasks_partial_pass2",
	"GenerateBlockMultipassTasks_partual_pass3",
};
const char *g_profiling_task_names_full[VoxelGeneratorMultipass::MAX_PASSES] = {
	"GenerateBlockMultipassTasks_full_pass0",
	"GenerateBlockMultipassTasks_full_pass1",
	"GenerateBlockMultipassTasks_full_pass2",
	"GenerateBlockMultipassTasks_full_pass3",
};

} // namespace

GenerateBlockMultipassTask::GenerateBlockMultipassTask(Vector3i p_block_position, uint8_t p_block_size,
		uint8_t p_pass_index, Ref<VoxelGeneratorMultipass> p_generator, bool full, IThreadedTask *p_caller,
		std::shared_ptr<std::atomic_int> p_caller_dependency_count) {
	//
	_block_position = p_block_position;
	_block_size = p_block_size;
	_pass_index = p_pass_index;
	_full = full;
	_generator = p_generator;
	_caller_task = p_caller;
	ZN_ASSERT(p_caller_dependency_count != nullptr);
	ZN_ASSERT(*p_caller_dependency_count > 0);
	_caller_task_dependency_counter = p_caller_dependency_count;

	if (_full) {
		int64_t v = ++g_task_full_count[_pass_index];
		ZN_PROFILE_PLOT(g_profiling_task_names_full[_pass_index], v);
	} else {
		int64_t v = ++g_task_partial_count[_pass_index];
		ZN_PROFILE_PLOT(g_profiling_task_names_partial[_pass_index], v);
	}
}

GenerateBlockMultipassTask::~GenerateBlockMultipassTask() {
	// Don't loose the caller task, it needs to be resumed or properly cancelled.
	ZN_ASSERT(_caller_task == nullptr);

	if (_caller_task_dependency_counter.use_count() == 1) {
		ZN_ASSERT(*_caller_task_dependency_counter == 0);
	}

	if (_full) {
		int64_t v = --g_task_full_count[_pass_index];
		ZN_PROFILE_PLOT(g_profiling_task_names_full[_pass_index], v);
	} else {
		int64_t v = --g_task_partial_count[_pass_index];
		ZN_PROFILE_PLOT(g_profiling_task_names_partial[_pass_index], v);
	}
}

void GenerateBlockMultipassTask::run(ThreadedTaskContext &ctx) {
	ZN_PROFILE_SCOPE();
	ZN_ASSERT(_generator.is_valid());

	std::shared_ptr<VoxelGeneratorMultipass::Map> map = _generator->get_map();

	const VoxelGeneratorMultipass::Pass &pass = _generator->get_pass(_pass_index);
	ZN_ASSERT(Vector3iUtil::is_valid_size(pass.dependency_extents.get_size()));

	if (_pass_index == 0) {
		// The first pass can't depend on another pass
		ZN_ASSERT(pass.dependency_extents.is_empty());
	}

	// TODO Handle column-based passes

	const Vector3i bpos_min = _block_position + pass.dependency_extents.min_pos;
	const Vector3i bpos_max = _block_position + pass.dependency_extents.max_pos;
	const Box3i neighbors_box = Box3i::from_min_max(bpos_min, bpos_max + Vector3i(1, 1, 1));

	const unsigned int central_block_index =
			Vector3iUtil::get_zxy_index(-pass.dependency_extents.min_pos, neighbors_box.size);

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

			// TODO Profile this. If it's a bottleneck, use postponing instead
			// Locking for writing because we can create new blocks.
			MutexLock mlock(map->mutex);

			// TODO Handle world bounds
			Vector3i bpos;
			for (bpos.z = bpos_min.z; bpos.z <= bpos_max.z; ++bpos.z) {
				for (bpos.x = bpos_min.x; bpos.x <= bpos_max.x; ++bpos.x) {
					for (bpos.y = bpos_min.y; bpos.y <= bpos_max.y; ++bpos.y) {
						std::shared_ptr<VoxelGeneratorMultipass::Block> &block = map->blocks[bpos];
						if (block == nullptr) {
							block = make_shared_instance<VoxelGeneratorMultipass::Block>();
						}
						blocks.push_back(block);
					}
				}
			}
		}

		struct BlockToGenerate {
			Vector3i position;
			std::shared_ptr<VoxelGeneratorMultipass::Block> block;
		};
		std::vector<BlockToGenerate> to_generate;

		const int pass_index = _pass_index;
		const int prev_pass_index = pass_index - 1;

		// Check loading levels
		{
			ZN_PROFILE_SCOPE_NAMED("Check levels");

			int prev_pass_completion_iterations = 0;
			if (prev_pass_index >= 0) {
				const VoxelGeneratorMultipass::Pass &prev_pass = _generator->get_pass(prev_pass_index);
				// TODO This should be lower at world boundaries
				prev_pass_completion_iterations =
						Vector3iUtil::get_volume(prev_pass.dependency_extents.get_size() + Vector3i(1, 1, 1));
			}

			Vector3i bpos;
			unsigned int i = 0;
			for (bpos.z = bpos_min.z; bpos.z <= bpos_max.z; ++bpos.z) {
				for (bpos.x = bpos_min.x; bpos.x <= bpos_max.x; ++bpos.x) {
					for (bpos.y = bpos_min.y; bpos.y <= bpos_max.y; ++bpos.y) {
						std::shared_ptr<VoxelGeneratorMultipass::Block> block = blocks[i];
						ZN_ASSERT(block != nullptr);

						if (_full) {
							// We want all blocks in the neighborhood to be at the same pass (but not necessarily
							// complete)
							if (block->pass_index < pass_index) {
								// Request partial pass
								to_generate.push_back({ bpos, block });
							}

						} else {
							// We want all blocks to have at least completed their previous pass
							if (block->pass_index < prev_pass_index) {
								// Request full pass
								to_generate.push_back({ bpos, block });

							} else if (block->pass_index == prev_pass_index) {
								// If this fails, something went wrong, it means a chunk was actually processed twice
								ZN_ASSERT(block->pass_iterations[prev_pass_index] <= prev_pass_completion_iterations);

								if (block->pass_iterations[prev_pass_index] != prev_pass_completion_iterations) {
									// Request full pass, even though that block has been processed.
									// It should at least generate relevant neighbors so it can be considered complete.
									to_generate.push_back({ bpos, block });
									// This sounds like it would be very rare though... also I don't like having to keep
									// caches around, it sounds easy to break...
								}
							} else {
								// Already processed to current pass or greater?
							}
						}

						++i;
					}
				}
			}

			if (_full && to_generate.size() == 1 && to_generate[0].block->pass_index == prev_pass_index &&
					to_generate[0].position == _block_position) {
				// This is the central block, we can process it in the current task without having to spawn
				// another.
				to_generate.clear();
			}
		}

		BufferedTaskScheduler &scheduler = BufferedTaskScheduler::get_for_current_thread();

		if (to_generate.size() > 0) {
			ZN_ASSERT(_pass_index > 0);

			// If we come back here after having requested dependencies (and we never postpone), then it means
			// tasks we spawned or were waiting for have not fulfilled our expectations, so something must be wrong
			ZN_ASSERT(!_debug_has_requested_deps);

			ZN_PROFILE_SCOPE_NAMED("Spawn subtasks");
			// Generate dependencies

			if (!_full) {
				ZN_ASSERT(_pass_index > 0);
			}

			// If we want the current chunk to be generated fully at the current pass, we must request all neighbors
			// that can reach it to be processed at least partially at that same pass.
			// If we are only processing the current chunk partially, we instead need to process neighbors that it wants
			// to reach at least up to the previous pass, and fully.
			const unsigned int requested_pass_index = _full ? _pass_index : _pass_index - 1;
			const bool requested_full = !_full;

			std::shared_ptr<std::atomic_int> counter = make_shared_instance<std::atomic_int>(to_generate.size());

			for (const BlockToGenerate &todo : to_generate) {
				GenerateBlockMultipassTask *task = ZN_NEW(GenerateBlockMultipassTask(
						todo.position, _block_size, requested_pass_index, _generator, requested_full, this, counter));
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

			if (main_block->pass_index == prev_pass_index) {
				if (_pass_index == 0) {
					// First pass creates the block
					for (std::shared_ptr<VoxelGeneratorMultipass::Block> block : blocks) {
						block->voxels.create(Vector3iUtil::create(_block_size));
					}
				}

				// Debug check
				const int pass_completion_iterations =
						Vector3iUtil::get_volume(pass.dependency_extents.get_size() + Vector3i(1, 1, 1));
				ZN_ASSERT(main_block->pass_iterations[_pass_index] < pass_completion_iterations);

				VoxelGeneratorMultipass::PassInput input;
				input.grid = to_span(blocks);
				input.grid_size = neighbors_box.size;
				input.grid_origin = neighbors_box.pos;
				input.main_block_position = _block_position;
				input.pass_index = pass_index;

				_generator->generate_pass(input);

				// Update levels
				main_block->pass_index = _pass_index;

				for (std::shared_ptr<VoxelGeneratorMultipass::Block> block : blocks) {
					block->pass_iterations[_pass_index]++;
				}

				// Pass, X, Y, Z, Time
				// println(format("P {} {} {} {} {}", int(_pass_index), _block_position.x, _block_position.y,
				// 		_block_position.z, Time::get_singleton()->get_ticks_usec()));

			} else if (main_block->pass_index == pass_index) {
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
