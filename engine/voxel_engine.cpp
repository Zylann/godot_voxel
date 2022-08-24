#include "voxel_engine.h"
#include "../constants/voxel_constants.h"
#include "../util/log.h"
#include "../util/macros.h"
#include "../util/profiling.h"
#include "../util/string_funcs.h"
#include "generate_block_task.h"
#include "load_all_blocks_data_task.h"
#include "load_block_data_task.h"
#include "mesh_block_task.h"
#include "save_block_data_task.h"

namespace zylann::voxel {

VoxelEngine *g_voxel_engine = nullptr;

VoxelEngine &VoxelEngine::get_singleton() {
	ZN_ASSERT_MSG(g_voxel_engine != nullptr, "Accessing singleton while it's null");
	return *g_voxel_engine;
}

void VoxelEngine::create_singleton(ThreadsConfig threads_config) {
	ZN_ASSERT_MSG(g_voxel_engine == nullptr, "Creating singleton twice");
	g_voxel_engine = ZN_NEW(VoxelEngine(threads_config));
}

void VoxelEngine::destroy_singleton() {
	ZN_ASSERT_MSG(g_voxel_engine != nullptr, "Destroying singleton twice");
	ZN_DELETE(g_voxel_engine);
	g_voxel_engine = nullptr;
}

VoxelEngine::VoxelEngine(ThreadsConfig threads_config) {
	const int hw_threads_hint = Thread::get_hardware_concurrency();
	ZN_PRINT_VERBOSE(format("Voxel: HW threads hint: {}", hw_threads_hint));

	ZN_ASSERT(threads_config.thread_count_margin_below_max >= 0);
	ZN_ASSERT(threads_config.thread_count_minimum >= 1);
	ZN_ASSERT(threads_config.thread_count_ratio_over_max >= 0.f);

	// Compute thread count for general pool.
	// Note that the I/O thread counts as one used thread and will always be present.

	const int maximum_thread_count = math::max(
			hw_threads_hint - threads_config.thread_count_margin_below_max, threads_config.thread_count_minimum);
	const int thread_count_by_ratio =
			int(Math::round(float(threads_config.thread_count_ratio_over_max) * hw_threads_hint));
	const int thread_count =
			math::clamp(thread_count_by_ratio, threads_config.thread_count_minimum, maximum_thread_count);
	ZN_PRINT_VERBOSE(format("Voxel: automatic thread count set to {}", thread_count));

	if (thread_count > hw_threads_hint) {
		ZN_PRINT_WARNING("Configured thread count exceeds hardware thread count. Performance may not be optimal");
	}

	_general_thread_pool.set_name("Voxel general");
	_general_thread_pool.set_thread_count(thread_count);
	_general_thread_pool.set_priority_update_period(200);

	// Init world
	_world.shared_priority_dependency = make_shared_instance<PriorityDependency::ViewersData>();

	ZN_PRINT_VERBOSE(format("Size of LoadBlockDataTask: {}", sizeof(LoadBlockDataTask)));
	ZN_PRINT_VERBOSE(format("Size of SaveBlockDataTask: {}", sizeof(SaveBlockDataTask)));
	ZN_PRINT_VERBOSE(format("Size of MeshBlockTask: {}", sizeof(MeshBlockTask)));
}

VoxelEngine::~VoxelEngine() {
	// The GDScriptLanguage singleton can get destroyed before ours, so any script referenced by tasks
	// cannot be freed. To work this around, tasks are cleared when the scene tree autoload is destroyed.
	// So normally there should not be any task left to clear here,
	// but doing it anyways for correctness, it's how it should have been...
	// See https://github.com/Zylann/godot_voxel/issues/189
	wait_and_clear_all_tasks(true);
}

void VoxelEngine::wait_and_clear_all_tasks(bool warn) {
	_general_thread_pool.wait_for_all_tasks();

	_general_thread_pool.dequeue_completed_tasks([warn](zylann::IThreadedTask *task) {
		if (warn) {
			ZN_PRINT_WARNING("General tasks remain on module cleanup, "
							 "this could become a problem if they reference scripts");
		}
		memdelete(task);
	});
}

uint32_t VoxelEngine::add_volume(VolumeCallbacks callbacks) {
	ZN_ASSERT(callbacks.check_callbacks());
	Volume volume;
	volume.callbacks = callbacks;
	return _world.volumes.create(volume);
}

VoxelEngine::VolumeCallbacks VoxelEngine::get_volume_callbacks(uint32_t volume_id) const {
	const Volume &volume = _world.volumes.get(volume_id);
	return volume.callbacks;
}

void VoxelEngine::remove_volume(uint32_t volume_id) {
	_world.volumes.destroy(volume_id);
	// TODO How to cancel meshing tasks?

	if (_world.volumes.count() == 0) {
		// To workaround https://github.com/Zylann/godot_voxel/issues/189
		// When the last remaining volume got destroyed (as in game exit)
		wait_and_clear_all_tasks(false);
	}
}

bool VoxelEngine::is_volume_valid(uint32_t volume_id) const {
	return _world.volumes.is_valid(volume_id);
}

uint32_t VoxelEngine::add_viewer() {
	return _world.viewers.create(Viewer());
}

void VoxelEngine::remove_viewer(uint32_t viewer_id) {
	_world.viewers.destroy(viewer_id);
}

void VoxelEngine::set_viewer_position(uint32_t viewer_id, Vector3 position) {
	Viewer &viewer = _world.viewers.get(viewer_id);
	viewer.world_position = position;
}

void VoxelEngine::set_viewer_distance(uint32_t viewer_id, unsigned int distance) {
	Viewer &viewer = _world.viewers.get(viewer_id);
	viewer.view_distance = distance;
}

unsigned int VoxelEngine::get_viewer_distance(uint32_t viewer_id) const {
	const Viewer &viewer = _world.viewers.get(viewer_id);
	return viewer.view_distance;
}

void VoxelEngine::set_viewer_requires_visuals(uint32_t viewer_id, bool enabled) {
	Viewer &viewer = _world.viewers.get(viewer_id);
	viewer.require_visuals = enabled;
}

bool VoxelEngine::is_viewer_requiring_visuals(uint32_t viewer_id) const {
	const Viewer &viewer = _world.viewers.get(viewer_id);
	return viewer.require_visuals;
}

void VoxelEngine::set_viewer_requires_collisions(uint32_t viewer_id, bool enabled) {
	Viewer &viewer = _world.viewers.get(viewer_id);
	viewer.require_collisions = enabled;
}

bool VoxelEngine::is_viewer_requiring_collisions(uint32_t viewer_id) const {
	const Viewer &viewer = _world.viewers.get(viewer_id);
	return viewer.require_collisions;
}

void VoxelEngine::set_viewer_requires_data_block_notifications(uint32_t viewer_id, bool enabled) {
	Viewer &viewer = _world.viewers.get(viewer_id);
	viewer.requires_data_block_notifications = enabled;
}

bool VoxelEngine::is_viewer_requiring_data_block_notifications(uint32_t viewer_id) const {
	const Viewer &viewer = _world.viewers.get(viewer_id);
	return viewer.requires_data_block_notifications;
}

void VoxelEngine::set_viewer_network_peer_id(uint32_t viewer_id, int peer_id) {
	Viewer &viewer = _world.viewers.get(viewer_id);
	viewer.network_peer_id = peer_id;
}

int VoxelEngine::get_viewer_network_peer_id(uint32_t viewer_id) const {
	const Viewer &viewer = _world.viewers.get(viewer_id);
	return viewer.network_peer_id;
}

bool VoxelEngine::viewer_exists(uint32_t viewer_id) const {
	return _world.viewers.is_valid(viewer_id);
}

void VoxelEngine::push_main_thread_time_spread_task(
		zylann::ITimeSpreadTask *task, TimeSpreadTaskRunner::Priority priority) {
	_time_spread_task_runner.push(task, priority);
}

void VoxelEngine::push_main_thread_progressive_task(zylann::IProgressiveTask *task) {
	_progressive_task_runner.push(task);
}

int VoxelEngine::get_main_thread_time_budget_usec() const {
	return _main_thread_time_budget_usec;
}

void VoxelEngine::set_main_thread_time_budget_usec(unsigned int usec) {
	_main_thread_time_budget_usec = usec;
}

void VoxelEngine::set_threaded_graphics_resource_building_enabled(bool enable) {
	_threaded_graphics_resource_building_enabled = enable;
}

bool VoxelEngine::is_threaded_graphics_resource_building_enabled() const {
	return _threaded_graphics_resource_building_enabled;
}

void VoxelEngine::push_async_task(zylann::IThreadedTask *task) {
	_general_thread_pool.enqueue(task, false);
}

void VoxelEngine::push_async_tasks(Span<zylann::IThreadedTask *> tasks) {
	_general_thread_pool.enqueue(tasks, false);
}

void VoxelEngine::push_async_io_task(zylann::IThreadedTask *task) {
	// I/O tasks run in serial because they usually can't run well in parallel due to locking shared resources.
	_general_thread_pool.enqueue(task, true);
}

void VoxelEngine::push_async_io_tasks(Span<zylann::IThreadedTask *> tasks) {
	_general_thread_pool.enqueue(tasks, true);
}

void VoxelEngine::process() {
	ZN_PROFILE_SCOPE();
	ZN_PROFILE_PLOT("Static memory usage", int64_t(OS::get_singleton()->get_static_memory_usage()));
	ZN_PROFILE_PLOT("TimeSpread tasks", int64_t(_time_spread_task_runner.get_pending_count()));
	ZN_PROFILE_PLOT("Progressive tasks", int64_t(_progressive_task_runner.get_pending_count()));

	// Receive generation and meshing results
	_general_thread_pool.dequeue_completed_tasks([](zylann::IThreadedTask *task) {
		task->apply_result();
		memdelete(task);
	});

	// Run this after dequeueing threaded tasks, because they can add some to this runner,
	// which could in turn complete right away (we avoid 1-frame delays this way).
	_time_spread_task_runner.process(_main_thread_time_budget_usec);

	_progressive_task_runner.process();

	// Update viewer dependencies
	{
		const size_t viewer_count = _world.viewers.count();
		if (_world.shared_priority_dependency->viewers.size() != viewer_count) {
			// TODO We can avoid the invalidation by using an atomic size or memory barrier?
			_world.shared_priority_dependency = make_shared_instance<PriorityDependency::ViewersData>();
			_world.shared_priority_dependency->viewers.resize(viewer_count);
		}
		size_t i = 0;
		unsigned int max_distance = 0;
		_world.viewers.for_each([&i, &max_distance, this](Viewer &viewer) {
			_world.shared_priority_dependency->viewers[i] = viewer.world_position;
			if (viewer.view_distance > max_distance) {
				max_distance = viewer.view_distance;
			}
			++i;
		});
		// Cancel distance is increased because of two reasons:
		// - Some volumes use a cubic area which has higher distances on their corners
		// - Hysteresis is needed to reduce ping-pong
		_world.shared_priority_dependency->highest_view_distance = max_distance * 2;
	}
}

static unsigned int debug_get_active_thread_count(const zylann::ThreadedTaskRunner &pool) {
	unsigned int active_count = 0;
	for (unsigned int i = 0; i < pool.get_thread_count(); ++i) {
		zylann::ThreadedTaskRunner::State s = pool.get_thread_debug_state(i);
		if (s == zylann::ThreadedTaskRunner::STATE_RUNNING) {
			++active_count;
		}
	}
	return active_count;
}

static VoxelEngine::Stats::ThreadPoolStats debug_get_pool_stats(const zylann::ThreadedTaskRunner &pool) {
	VoxelEngine::Stats::ThreadPoolStats d;
	d.tasks = pool.get_debug_remaining_tasks();
	d.active_threads = debug_get_active_thread_count(pool);
	d.thread_count = pool.get_thread_count();
	return d;
}

VoxelEngine::Stats VoxelEngine::get_stats() const {
	Stats s;
	s.general = debug_get_pool_stats(_general_thread_pool);
	s.generation_tasks = GenerateBlockTask::debug_get_running_count();
	s.meshing_tasks = GenerateBlockTask::debug_get_running_count();
	s.streaming_tasks = LoadBlockDataTask::debug_get_running_count() + SaveBlockDataTask::debug_get_running_count();
	s.main_thread_tasks = _time_spread_task_runner.get_pending_count() + _progressive_task_runner.get_pending_count();
	return s;
}

BufferedTaskScheduler &BufferedTaskScheduler::get_for_current_thread() {
	static thread_local BufferedTaskScheduler tls_task_scheduler;
	return tls_task_scheduler;
}

} // namespace zylann::voxel
