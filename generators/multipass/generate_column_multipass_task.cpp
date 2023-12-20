#include "generate_column_multipass_task.h"
#include "../../engine/voxel_engine.h"
#include "../../storage/voxel_data.h"

#include "../../util/dstack.h"
#include "../../util/godot/classes/time.h"
#include "../../util/string_funcs.h"

namespace zylann::voxel {

namespace {
#ifdef ZN_PROFILER_ENABLED
std::atomic_int g_task_count[VoxelGeneratorMultipassCB::MAX_SUBPASSES] = { 0 };
const char *g_profiling_task_names[VoxelGeneratorMultipassCB::MAX_SUBPASSES] = {
	"GenerateColumnMultipassTasks_subpass0",
	"GenerateColumnMultipassTasks_subpass1",
	"GenerateColumnMultipassTasks_subpass2",
	"GenerateColumnMultipassTasks_subpass3",
	"GenerateColumnMultipassTasks_subpass4",
	"GenerateColumnMultipassTasks_subpass5",
	"GenerateColumnMultipassTasks_subpass6",
};
#endif

} // namespace

using namespace VoxelGeneratorMultipassCBStructs;

GenerateColumnMultipassTask::GenerateColumnMultipassTask(Vector2i p_column_position, uint8_t p_block_size,
		uint8_t p_subpass_index, std::shared_ptr<Internal> p_generator_internal,
		Ref<VoxelGeneratorMultipassCB> p_generator, TaskPriority p_priority, IThreadedTask *p_caller,
		std::shared_ptr<std::atomic_int> p_caller_dependency_count) {
	//
	_column_position = p_column_position;
	_priority = p_priority;
	_block_size = p_block_size;
	_subpass_index = p_subpass_index;

	ZN_ASSERT(p_generator.is_valid());
	_generator = p_generator;

	ZN_ASSERT(p_generator_internal != nullptr);
	_generator_internal = p_generator_internal;

	ZN_ASSERT(p_caller != nullptr);
	_caller_task = p_caller;
	_caller_task_dependency_counter = p_caller_dependency_count;

#ifdef ZN_PROFILER_ENABLED
	int64_t v = ++g_task_count[_subpass_index];
	ZN_PROFILE_PLOT(g_profiling_task_names[_subpass_index], v);
#endif

	// println(format("K {} {} {} {} {}", int(_subpass_index), _column_position.x, 0, _column_position.y,
	// 		Time::get_singleton()->get_ticks_usec()));
}

GenerateColumnMultipassTask::~GenerateColumnMultipassTask() {
	ZN_ASSERT(_caller_task == nullptr);

#ifdef ZN_PROFILER_ENABLED
	int64_t v = --g_task_count[_subpass_index];
	ZN_PROFILE_PLOT(g_profiling_task_names[_subpass_index], v);
#endif

	// println(format("J {} {} {} {} {}", int(_subpass_index), _column_position.x, 0, _column_position.y,
	// 		Time::get_singleton()->get_ticks_usec()));
}

void GenerateColumnMultipassTask::run(ThreadedTaskContext &ctx) {
	ZN_DSTACK();
	ZN_PROFILE_SCOPE();
	ZN_ASSERT(_generator != nullptr);

	Map &map = _generator_internal->map;
	BufferedTaskScheduler &task_scheduler = BufferedTaskScheduler::get_for_current_thread();

	const int final_subpass_index =
			VoxelGeneratorMultipassCB::get_subpass_count_from_pass_count(_generator_internal->passes.size()) - 1;

	if (_cancelled) {
		// At least one subtask was cancelled, therefore we have to cleanup and return too.

		// Unregister from the column if any
		{
			if (!map.spatial_lock.try_lock_write(BoxBounds2i::from_position(_column_position))) {
				// Try later (funny situation, but that's the pattern)
				ctx.status = ThreadedTaskContext::STATUS_POSTPONED;
				return;
			}
			SpatialLock2D::UnlockWriteOnScopeExit swlock(
					map.spatial_lock, BoxBounds2i::from_position(_column_position));

			MutexLock mlock(map.mutex);
			auto column_it = map.columns.find(_column_position);
			if (column_it != map.columns.end()) {
				// Unregister task from the column
				Column &column = column_it->second;
				column.pending_subpass_tasks_mask &= ~(1 << _subpass_index);

				if (_subpass_index == final_subpass_index) {
					// Schedule pending block requests to make them handle cancellation
					schedule_final_block_tasks(column, task_scheduler);
				}
			}
		}

		return_to_caller(false);
		task_scheduler.flush();
		return;
	}

	const int pass_index = VoxelGeneratorMultipassCB::get_pass_index_from_subpass(_subpass_index);
	const Pass &pass = _generator_internal->passes[pass_index];

	if (_subpass_index == 0) {
		// The first subpass can't depend on another subpass
		ZN_ASSERT(pass.dependency_extents == 0);
	} else {
		ZN_ASSERT(pass.dependency_extents > 0);
	}

	const Box2i neighbors_box = Box2i::from_min_max( //
			_column_position - Vector2iUtil::create(pass.dependency_extents),
			_column_position + Vector2iUtil::create(pass.dependency_extents + 1));

	const unsigned int central_block_index =
			Vector2iUtil::get_yx_index(Vector2iUtil::create(pass.dependency_extents), neighbors_box.size);

	std::vector<Column *> columns;
	// TODO Cache memory
	columns.reserve(Vector2iUtil::get_area(neighbors_box.size));

	// Lock region we are going to process
	{
		ZN_PROFILE_SCOPE_NAMED("Region");

		// Blocking until available causes bottlenecks. Not always big ones, but enough to be very noticeable in the
		// profiler.
		// SpatialLock3D::Write swlock(map->spatial_lock, neighbors_box);
		if (!map.spatial_lock.try_lock_write(neighbors_box)) {
			// Try later
			ctx.status = ThreadedTaskContext::STATUS_POSTPONED;
			return;
		}
		// Sometimes I wish `defer` was a thing in C++
		SpatialLock2D::UnlockWriteOnScopeExit swlock(map.spatial_lock, neighbors_box);

		// Fetch columns from map
		{
			ZN_PROFILE_SCOPE_NAMED("Fetch columns");

			// TODO We don't create new columns from here, could use a shared lock?
			MutexLock mlock(map.mutex);

			Vector2i bpos;
			// Coordinate order matters (note, Y in Vector2i corresponds to Z in 3D here).
			neighbors_box.for_each_cell_yx([&columns, &map](Vector2i cpos) {
				auto it = map.columns.find(cpos);
				Column *column = nullptr;
				if (it != map.columns.end()) {
					column = &it->second;
				}
				columns.push_back(column);
			});
		}

		const int subpass_index = _subpass_index;
		const int prev_subpass_index = subpass_index - 1;

		Column *main_column = columns[central_block_index];

		bool spawned_subtasks = false;
		bool postpone = false;
		int debug_subtask_count = 0;

		// Check loading levels
		{
			ZN_PROFILE_SCOPE_NAMED("Check levels");

			Vector2i cpos;

			const Vector2i cpos_min = neighbors_box.pos;
			const Vector2i cpos_max = neighbors_box.pos + neighbors_box.size;

			std::shared_ptr<std::atomic_int> dependency_counter = nullptr;

			for (Column *column : columns) {
				if (column == nullptr) {
					// No longer loaded, we have to cancel the task

					if (main_column != nullptr) {
						main_column->pending_subpass_tasks_mask &= ~(1 << _subpass_index);

						if (_subpass_index == final_subpass_index) {
							// Schedule pending block requests to make them handle cancellation
							schedule_final_block_tasks(*main_column, task_scheduler);
						}
					}

					return_to_caller(false);
					task_scheduler.flush();
					return;
				}
			}

			// ZN_ASSERT(!has_duplicate(to_span_const(columns)));

			unsigned int i = 0;
			for (cpos.y = cpos_min.y; cpos.y < cpos_max.y; ++cpos.y) {
				for (cpos.x = cpos_min.x; cpos.x < cpos_max.x; ++cpos.x) {
					Column *column = columns[i];
					ZN_ASSERT(column != nullptr);

					// We want all blocks in the neighborhood to be at least at the previous subpass before we can
					// run the current subpass
					if (prev_subpass_index >= 0 && column->subpass_index < prev_subpass_index) {
						// Dependencies not ready yet.

						if (column->loading) {
							// A task is pending to work on the dependency, so we wait.
							// TODO Ideally we should subscribe to the completion of that task.
							// println(format("O {} {} {} {} {}", int(_subpass_index), _column_position.x, 0,
							// 		_column_position.y, Time::get_singleton()->get_ticks_usec()));
							postpone = true;

						} else if ((column->pending_subpass_tasks_mask & (1 << prev_subpass_index)) != 0) {
							// A task is pending to work on the dependency, so we wait.
							// TODO Ideally we should subscribe to the completion of that task.
							// We can do that
							// println(format("O {} {} {} {} {}", int(_subpass_index), _column_position.x, 0,
							// 		_column_position.y, Time::get_singleton()->get_ticks_usec()));
							postpone = true;

						} else {
							// No task is pending to work on the dependency, spawn one.

							if (dependency_counter == nullptr) {
								dependency_counter = make_shared_instance<std::atomic_int>();
							}
							++(*dependency_counter);

							GenerateColumnMultipassTask *subtask =
									ZN_NEW(GenerateColumnMultipassTask(cpos, _block_size, prev_subpass_index,
											_generator_internal, _generator, _priority, this, dependency_counter));
							subtask->_caller_mp_task = this;
							task_scheduler.push_main_task(subtask);

							column->pending_subpass_tasks_mask |= (1 << prev_subpass_index);

							spawned_subtasks = true;
							++debug_subtask_count;
						}
						// TODO If a column got deallocated after it was returned once, restart its generation process.
						// This would be to cover cases where blocks of a column get requested more than once. In the
						// ideal case this should not happen, but the real world is a mess:
						// - The game could have crashed
						// - Saving could have failed
						// - Files could have been deleted
						// - The game could simply want to reset an area
					}

					++i;
				}
			}
		}

		if (spawned_subtasks) {
			ctx.status = ThreadedTaskContext::STATUS_TAKEN_OUT;
			// println(format("task {} {} {} spawned {} subtasks", _subpass_index, _column_position.x,
			// _column_position.y, 		debug_subtask_count));

		} else if (postpone) {
			ctx.status = ThreadedTaskContext::STATUS_POSTPONED;
			return;

		} else {
			ZN_PROFILE_SCOPE_NAMED("Run pass");
			// We can run the pass

			ZN_ASSERT(main_column != nullptr);

			if (main_column->subpass_index == prev_subpass_index) {
				const int column_height_blocks = _generator_internal->column_height_blocks;

				if (_subpass_index == 0) {
					// First pass creates blocks
					// main_column->blocks.resize(column_height_blocks);
					for (Block &block : main_column->blocks) {
						block.voxels.create(Vector3iUtil::create(_block_size));
					}
				}

				// Debug check
				// const int subpass_completion_iterations = math::cubed(pass.dependency_extents * 2 + 1);
				// ZN_ASSERT(main_block->subpass_iterations[_subpass_index] < subpass_completion_iterations);

				// The fact we split passes in two doesn't mean we should run the generator again each time. Instead, we
				// may run the generator only on the first subpass referring to a given pass.
				// TODO that means part of the tasks we spawn don't actually do expensive work. Could we simplify this
				// further?
				const int prev_pass_index = VoxelGeneratorMultipassCB::get_pass_index_from_subpass(prev_subpass_index);

				if (pass_index == 0 || prev_pass_index != pass_index) {
					const int column_base_y_blocks = _generator_internal->column_base_y_blocks;

					// TODO Cache memory
					std::vector<Block *> blocks;
					blocks.reserve(columns.size() * column_height_blocks);
					// Compose grid of blocks indexed as ZXY (index+1 goes up along Y).
					// ZXY indexing is convenient here, since columns are indexed with YX (aka ZX, because Y in 2D is Z
					// in 3D)
					for (Column *column : columns) {
						for (Block &block : column->blocks) {
							blocks.push_back(&block);
						}
					}

					PassInput input;
					input.grid = to_span(blocks);
					input.grid_size = Vector3i(neighbors_box.size.x, column_height_blocks, neighbors_box.size.y);
					input.grid_origin = Vector3i(neighbors_box.pos.x, column_base_y_blocks, neighbors_box.pos.y);
					input.main_block_position = Vector3i(_column_position.x, column_base_y_blocks, _column_position.y);
					input.pass_index = pass_index;
					input.block_size = _block_size;

					// This should be the ONLY place where `_generator` is used.
					_generator->generate_pass(input);
				}

				// Update levels
				main_column->subpass_index = _subpass_index;

				// for (Column *column : columns) {
				// 	column->subpass_iterations[_subpass_index]++;
				// }

				// Pass, X, Y, Z, Time
				// println(format("P {} {} {} {} {}", int(_subpass_index), _column_position.x, 0, _column_position.y,
				// 		Time::get_singleton()->get_ticks_usec()));
			}

			main_column->pending_subpass_tasks_mask &= ~(1 << _subpass_index);

			if (main_column->subpass_index == final_subpass_index) {
				// All tasks that were waiting for this column to be complete (and did not spawn column subtasks
				// themselves) may now resume
				schedule_final_block_tasks(*main_column, task_scheduler);
			}

			return_to_caller(true);
		}

	} // Region lock

	// println(format("End of {} t {}", uint64_t(this), Thread::get_caller_id()));
	task_scheduler.flush();
}

void GenerateColumnMultipassTask::schedule_final_block_tasks(Column &column, BufferedTaskScheduler &task_scheduler) {
	for (Block &block : column.blocks) {
		if (block.final_pending_task != nullptr) {
			ZN_ASSERT(block.final_pending_task != _caller_task);
			task_scheduler.push_main_task(block.final_pending_task);
			block.final_pending_task = nullptr;
		}
	}
}

void GenerateColumnMultipassTask::return_to_caller(bool success) {
	ZN_ASSERT(_caller_task != nullptr);
	ZN_ASSERT(_caller_task_dependency_counter != nullptr);
	const int counter = --(*_caller_task_dependency_counter);
	ZN_ASSERT(counter >= 0);
	if (!success) {
		if (_caller_mp_task != nullptr) {
			_caller_mp_task->_cancelled = true;
		}
		// println(format("C {} {} {} {} {}", int(_subpass_index), _column_position.x, 0, _column_position.y,
		// 		Time::get_singleton()->get_ticks_usec()));
	}
	if (counter == 0) {
		VoxelEngine::get_singleton().push_async_task(_caller_task);
	}
	_caller_task = nullptr;
}

} // namespace zylann::voxel
