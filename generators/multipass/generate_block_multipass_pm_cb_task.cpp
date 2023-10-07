#if 0
#include "generate_block_multipass_pm_cb_task.h"
#include "../../engine/voxel_engine.h"
#include "../../storage/voxel_data.h"

#include "../../util/godot/classes/time.h"
#include "../../util/string_funcs.h"

namespace zylann::voxel {

namespace {
std::atomic_int g_task_count[VoxelGeneratorMultipassCB::MAX_SUBPASSES] = { 0 };
const char *g_profiling_task_names[VoxelGeneratorMultipassCB::MAX_SUBPASSES] = {
	"GenerateBlockMultipassPMCBTasks_subpass0",
	"GenerateBlockMultipassPMCBTasks_subpass1",
	"GenerateBlockMultipassPMCBTasks_subpass2",
	"GenerateBlockMultipassPMCBTasks_subpass3",
	"GenerateBlockMultipassPMCBTasks_subpass4",
	"GenerateBlockMultipassPMCBTasks_subpass5",
	"GenerateBlockMultipassPMCBTasks_subpass6",
};

} // namespace

GenerateBlockMultipassPMCBTask::GenerateBlockMultipassPMCBTask(Vector2i p_column_position, uint8_t p_block_size,
		uint8_t p_subpass_index, Ref<VoxelGeneratorMultipassCB> p_generator, TaskPriority p_priority) {
	//
	_column_position = p_column_position;
	_block_size = p_block_size;
	_subpass_index = p_subpass_index;

	ZN_ASSERT(p_generator.is_valid());
	_generator = p_generator;

	_priority = p_priority;

	int64_t v = ++g_task_count[_subpass_index];
	ZN_PROFILE_PLOT(g_profiling_task_names[_subpass_index], v);
}

GenerateBlockMultipassPMCBTask::~GenerateBlockMultipassPMCBTask() {
	int64_t v = --g_task_count[_subpass_index];
	ZN_PROFILE_PLOT(g_profiling_task_names[_subpass_index], v);
}

void GenerateBlockMultipassPMCBTask::run(ThreadedTaskContext &ctx) {
	ZN_PROFILE_SCOPE();
	ZN_ASSERT(_generator.is_valid());

	const int pass_index = VoxelGeneratorMultipassCB::get_pass_index_from_subpass(_subpass_index);
	const VoxelGeneratorMultipassCB::Pass &pass = _generator->get_pass(pass_index);

	const int final_subpass_index =
			VoxelGeneratorMultipassCB::get_subpass_count_from_pass_count(_generator->get_pass_count()) - 1;

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

	std::vector<VoxelGeneratorMultipassCB::Column *> columns;
	// TODO Cache memory
	columns.reserve(Vector2iUtil::get_area(neighbors_box.size));

	std::shared_ptr<VoxelGeneratorMultipassCB::Map> map = _generator->get_map();

	IThreadedTask *next_task = nullptr;

	BufferedTaskScheduler &task_scheduler = BufferedTaskScheduler::get_for_current_thread();

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
		SpatialLock2D::UnlockWriteOnScopeExit swlock(map->spatial_lock, neighbors_box);

		// Fetch columns from map
		{
			ZN_PROFILE_SCOPE_NAMED("Fetch columns");

			// TODO We don't create new columns from here, could use a shared lock?
			MutexLock mlock(map->mutex);

			// TODO Handle world bounds
			Vector2i bpos;
			// Coordinate order matters
			neighbors_box.for_each_cell_yx([&columns, map](Vector2i cpos) {
				auto it = map->columns.find(cpos);
				VoxelGeneratorMultipassCB::Column *column = nullptr;
				if (it != map->columns.end()) {
					column = &it->second;
				}
				columns.push_back(column);
			});
		}

		const int subpass_index = _subpass_index;
		const int prev_subpass_index = subpass_index - 1;

		VoxelGeneratorMultipassCB::Column *main_column = columns[central_block_index];

		// Check loading levels
		{
			ZN_PROFILE_SCOPE_NAMED("Check levels");

			Vector2i cpos;
			unsigned int i = 0;

			const Vector2i cpos_min = neighbors_box.pos;
			const Vector2i cpos_max = neighbors_box.pos + neighbors_box.size;

			for (cpos.y = cpos_min.y; cpos.y < cpos_max.y; ++cpos.y) {
				for (cpos.x = cpos_min.x; cpos.x < cpos_max.x; ++cpos.x) {
					VoxelGeneratorMultipassCB::Column *column = columns[i];
					if (column == nullptr) {
						// No longer loaded, we have to cancel the task

						if (main_column != nullptr) {
							main_column->pending_subpass_tasks_mask &= ~(1 << _subpass_index);
						}
						// println(format("C {} {} {} {} {}", int(_subpass_index), _column_position.x, 0,
						// 		_column_position.y, Time::get_singleton()->get_ticks_usec()));
						return;
					}

					// We want all blocks in the neighborhood to be at least at the previous subpass before we can
					// run the current subpass
					if (prev_subpass_index >= 0 && column->subpass_index < prev_subpass_index) {
						// Dependencies not ready yet.

						if (column->loading || (column->pending_subpass_tasks_mask & (1 << prev_subpass_index)) != 0) {
							// A task is pending to work on the dependency, so we wait.
							ctx.status = ThreadedTaskContext::STATUS_POSTPONED;
							// println(format("O {} {} {} {} {}", int(_subpass_index), _column_position.x, 0,
							// 		_column_position.y, Time::get_singleton()->get_ticks_usec()));
							return;

						} else {
							// No task is pending to work on the dependency, we have to cancel.
							if (main_column != nullptr) {
								main_column->pending_subpass_tasks_mask &= ~(1 << _subpass_index);
							}
							// println(format("C {} {} {} {} {}", int(_subpass_index), _column_position.x, 0,
							// 		_column_position.y, Time::get_singleton()->get_ticks_usec()));
							return;
						}
					}

					++i;
				}
			}
		}

		{
			ZN_PROFILE_SCOPE_NAMED("Run pass");
			// We can run the pass

			ZN_ASSERT(main_column != nullptr);

			if (main_column->subpass_index == prev_subpass_index) {
				const int column_height_blocks = _generator->get_column_height_blocks();

				if (_subpass_index == 0) {
					// First pass creates blocks
					// main_column->blocks.resize(column_height_blocks);
					for (VoxelGeneratorMultipassCB::Block &block : main_column->blocks) {
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
					const int column_base_y_blocks = _generator->get_column_base_y_blocks();

					// TODO Cache memory
					std::vector<VoxelGeneratorMultipassCB::Block *> blocks;
					blocks.reserve(columns.size() * column_height_blocks);
					// Compose grid of blocks indexed as ZXY (index+1 goes up along Y).
					// ZXY indexing is convenient here, since columns are indexed with YX (where Y in 2D is Z in 3D)
					for (VoxelGeneratorMultipassCB::Column *column : columns) {
						for (VoxelGeneratorMultipassCB::Block &block : column->blocks) {
							blocks.push_back(&block);
						}
					}

					VoxelGeneratorMultipassCB::PassInput input;
					input.grid = to_span(blocks);
					input.grid_size = Vector3i(neighbors_box.size.x, column_height_blocks, neighbors_box.size.y);
					input.grid_origin = Vector3i(neighbors_box.pos.x, column_base_y_blocks, neighbors_box.pos.y);
					input.main_block_position = Vector3i(_column_position.x, column_base_y_blocks, _column_position.y);
					input.pass_index = pass_index;
					input.block_size = _block_size;

					_generator->generate_pass(input);
				}

				// Update levels
				main_column->subpass_index = _subpass_index;

				for (VoxelGeneratorMultipassCB::Column *column : columns) {
					column->subpass_iterations[_subpass_index]++;
				}

				// Pass, X, Y, Z, Time
				// println(format("P {} {} {} {} {}", int(_subpass_index), _column_position.x, 0, _column_position.y,
				// 		Time::get_singleton()->get_ticks_usec()));
			}

			main_column->pending_subpass_tasks_mask &= ~(1 << _subpass_index);
		}

		if (main_column->subpass_index == final_subpass_index) {
			for (VoxelGeneratorMultipassCB::Block &block : main_column->blocks) {
				if (block.final_pending_task != nullptr) {
					task_scheduler.push_main_task(block.final_pending_task);
					block.final_pending_task = nullptr;
				}
			}
		}

	} // Region lock

	task_scheduler.flush();
}

} // namespace zylann::voxel
#endif
