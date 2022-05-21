#include "voxel_server.h"
#include "../constants/voxel_constants.h"
#include "../storage/voxel_memory_pool.h"
#include "../util/log.h"
#include "../util/macros.h"
#include "../util/profiling.h"
#include "../util/string_funcs.h"
#include "generate_block_task.h"
#include "load_all_blocks_data_task.h"
#include "load_block_data_task.h"
#include "mesh_block_task.h"
#include "save_block_data_task.h"

#include <core/config/project_settings.h>

namespace zylann::voxel {

VoxelServer *g_voxel_server = nullptr;

VoxelServer &VoxelServer::get_singleton() {
	ZN_ASSERT_MSG(g_voxel_server != nullptr, "Accessing singleton while it's null");
	return *g_voxel_server;
}

void VoxelServer::create_singleton() {
	ZN_ASSERT_MSG(g_voxel_server == nullptr, "Creating singleton twice");
	g_voxel_server = memnew(VoxelServer);
}

void VoxelServer::destroy_singleton() {
	ZN_ASSERT_MSG(g_voxel_server != nullptr, "Destroying singleton twice");
	memdelete(g_voxel_server);
	g_voxel_server = nullptr;
}

VoxelServer::VoxelServer() {
	CRASH_COND(ProjectSettings::get_singleton() == nullptr);

	const int hw_threads_hint = Thread::get_hardware_concurrency();
	ZN_PRINT_VERBOSE(format("Voxel: HW threads hint: {}", hw_threads_hint));

	// Compute thread count for general pool.
	// Note that the I/O thread counts as one used thread and will always be present.

	// "RST" means changing the property requires an editor restart (or game restart)
	GLOBAL_DEF_RST("voxel/threads/count/minimum", 1);
	ProjectSettings::get_singleton()->set_custom_property_info("voxel/threads/count/minimum",
			PropertyInfo(Variant::INT, "voxel/threads/count/minimum", PROPERTY_HINT_RANGE, "1,64"));

	GLOBAL_DEF_RST("voxel/threads/count/margin_below_max", 1);
	ProjectSettings::get_singleton()->set_custom_property_info("voxel/threads/count/margin_below_max",
			PropertyInfo(Variant::INT, "voxel/threads/count/margin_below_max", PROPERTY_HINT_RANGE, "1,64"));

	GLOBAL_DEF_RST("voxel/threads/count/ratio_over_max", 0.5f);
	ProjectSettings::get_singleton()->set_custom_property_info("voxel/threads/count/ratio_over_max",
			PropertyInfo(Variant::FLOAT, "voxel/threads/count/ratio_over_max", PROPERTY_HINT_RANGE, "0,1,0.1"));

	GLOBAL_DEF_RST("voxel/threads/main/time_budget_ms", 8);
	ProjectSettings::get_singleton()->set_custom_property_info("voxel/threads/main/time_budget_ms",
			PropertyInfo(Variant::INT, "voxel/threads/main/time_budget_ms", PROPERTY_HINT_RANGE, "0,1000"));

	_main_thread_time_budget_usec =
			1000 * int(ProjectSettings::get_singleton()->get("voxel/threads/main/time_budget_ms"));

	const int minimum_thread_count =
			math::max(1, int(ProjectSettings::get_singleton()->get("voxel/threads/count/minimum")));

	// How many threads below available count on the CPU should we set as limit
	const int thread_count_margin =
			math::max(1, int(ProjectSettings::get_singleton()->get("voxel/threads/count/margin_below_max")));

	// Portion of available CPU threads to attempt using
	const float threads_ratio =
			math::clamp(float(ProjectSettings::get_singleton()->get("voxel/threads/count/ratio_over_max")), 0.f, 1.f);

	const int maximum_thread_count = math::max(hw_threads_hint - thread_count_margin, minimum_thread_count);
	// `-1` is for the stream thread
	const int thread_count_by_ratio = int(Math::round(float(threads_ratio) * hw_threads_hint)) - 1;
	const int thread_count = math::clamp(thread_count_by_ratio, minimum_thread_count, maximum_thread_count);
	ZN_PRINT_VERBOSE(format("Voxel: automatic thread count set to {}", thread_count));

	if (thread_count > hw_threads_hint) {
		WARN_PRINT("Configured thread count exceeds hardware thread count. Performance may not be optimal");
	}

	// I/O can't be more than 1 thread. File access with more threads isn't worth it.
	// This thread isn't configurable at the moment.
	_streaming_thread_pool.set_name("Voxel streaming");
	_streaming_thread_pool.set_thread_count(1);
	_streaming_thread_pool.set_priority_update_period(300);
	// Batching is only to give a chance for file I/O tasks to be grouped and reduce open/close calls.
	// But in the end it might be better to move this idea to the tasks themselves?
	_streaming_thread_pool.set_batch_count(16);

	_general_thread_pool.set_name("Voxel general");
	_general_thread_pool.set_thread_count(thread_count);
	_general_thread_pool.set_priority_update_period(200);
	_general_thread_pool.set_batch_count(1);

	// Init world
	_world.shared_priority_dependency = make_shared_instance<PriorityDependency::ViewersData>();

	ZN_PRINT_VERBOSE(format("Size of LoadBlockDataTask: {}", sizeof(LoadBlockDataTask)));
	ZN_PRINT_VERBOSE(format("Size of SaveBlockDataTask: {}", sizeof(SaveBlockDataTask)));
	ZN_PRINT_VERBOSE(format("Size of MeshBlockTask: {}", sizeof(MeshBlockTask)));
}

VoxelServer::~VoxelServer() {
	// The GDScriptLanguage singleton can get destroyed before ours, so any script referenced by tasks
	// cannot be freed. To work this around, tasks are cleared when the scene tree autoload is destroyed.
	// So normally there should not be any task left to clear here,
	// but doing it anyways for correctness, it's how it should have been...
	// See https://github.com/Zylann/godot_voxel/issues/189
	wait_and_clear_all_tasks(true);
}

void VoxelServer::wait_and_clear_all_tasks(bool warn) {
	_streaming_thread_pool.wait_for_all_tasks();
	_general_thread_pool.wait_for_all_tasks();

	// Wait a second time because the generation pool can generate streaming requests
	_streaming_thread_pool.wait_for_all_tasks();

	_streaming_thread_pool.dequeue_completed_tasks([warn](zylann::IThreadedTask *task) {
		if (warn) {
			WARN_PRINT("Streaming tasks remain on module cleanup, "
					   "this could become a problem if they reference scripts");
		}
		memdelete(task);
	});

	_general_thread_pool.dequeue_completed_tasks([warn](zylann::IThreadedTask *task) {
		if (warn) {
			WARN_PRINT("General tasks remain on module cleanup, "
					   "this could become a problem if they reference scripts");
		}
		memdelete(task);
	});
}

uint32_t VoxelServer::add_volume(VolumeCallbacks callbacks, VolumeType type) {
	CRASH_COND(!callbacks.check_callbacks());
	Volume volume;
	volume.type = type;
	volume.callbacks = callbacks;
	// volume.meshing_dependency = make_shared_instance<MeshingDependency>();
	return _world.volumes.create(volume);
}

VoxelServer::VolumeCallbacks VoxelServer::get_volume_callbacks(uint32_t volume_id) const {
	const Volume &volume = _world.volumes.get(volume_id);
	return volume.callbacks;
}

void VoxelServer::remove_volume(uint32_t volume_id) {
	_world.volumes.destroy(volume_id);
	// TODO How to cancel meshing tasks?

	if (_world.volumes.count() == 0) {
		// To workaround https://github.com/Zylann/godot_voxel/issues/189
		// When the last remaining volume got destroyed (as in game exit)
		wait_and_clear_all_tasks(false);
	}
}

bool VoxelServer::is_volume_valid(uint32_t volume_id) const {
	return _world.volumes.is_valid(volume_id);
}

uint32_t VoxelServer::add_viewer() {
	return _world.viewers.create(Viewer());
}

void VoxelServer::remove_viewer(uint32_t viewer_id) {
	_world.viewers.destroy(viewer_id);
}

void VoxelServer::set_viewer_position(uint32_t viewer_id, Vector3 position) {
	Viewer &viewer = _world.viewers.get(viewer_id);
	viewer.world_position = position;
}

void VoxelServer::set_viewer_distance(uint32_t viewer_id, unsigned int distance) {
	Viewer &viewer = _world.viewers.get(viewer_id);
	viewer.view_distance = distance;
}

unsigned int VoxelServer::get_viewer_distance(uint32_t viewer_id) const {
	const Viewer &viewer = _world.viewers.get(viewer_id);
	return viewer.view_distance;
}

void VoxelServer::set_viewer_requires_visuals(uint32_t viewer_id, bool enabled) {
	Viewer &viewer = _world.viewers.get(viewer_id);
	viewer.require_visuals = enabled;
}

bool VoxelServer::is_viewer_requiring_visuals(uint32_t viewer_id) const {
	const Viewer &viewer = _world.viewers.get(viewer_id);
	return viewer.require_visuals;
}

void VoxelServer::set_viewer_requires_collisions(uint32_t viewer_id, bool enabled) {
	Viewer &viewer = _world.viewers.get(viewer_id);
	viewer.require_collisions = enabled;
}

bool VoxelServer::is_viewer_requiring_collisions(uint32_t viewer_id) const {
	const Viewer &viewer = _world.viewers.get(viewer_id);
	return viewer.require_collisions;
}

void VoxelServer::set_viewer_requires_data_block_notifications(uint32_t viewer_id, bool enabled) {
	Viewer &viewer = _world.viewers.get(viewer_id);
	viewer.requires_data_block_notifications = enabled;
}

bool VoxelServer::is_viewer_requiring_data_block_notifications(uint32_t viewer_id) const {
	const Viewer &viewer = _world.viewers.get(viewer_id);
	return viewer.requires_data_block_notifications;
}

void VoxelServer::set_viewer_network_peer_id(uint32_t viewer_id, int peer_id) {
	Viewer &viewer = _world.viewers.get(viewer_id);
	viewer.network_peer_id = peer_id;
}

int VoxelServer::get_viewer_network_peer_id(uint32_t viewer_id) const {
	const Viewer &viewer = _world.viewers.get(viewer_id);
	return viewer.network_peer_id;
}

bool VoxelServer::viewer_exists(uint32_t viewer_id) const {
	return _world.viewers.is_valid(viewer_id);
}

void VoxelServer::push_main_thread_time_spread_task(zylann::ITimeSpreadTask *task) {
	_time_spread_task_runner.push(task);
}

void VoxelServer::push_main_thread_progressive_task(zylann::IProgressiveTask *task) {
	_progressive_task_runner.push(task);
}

int VoxelServer::get_main_thread_time_budget_usec() const {
	return _main_thread_time_budget_usec;
}

void VoxelServer::push_async_task(zylann::IThreadedTask *task) {
	_general_thread_pool.enqueue(task);
}

void VoxelServer::push_async_tasks(Span<zylann::IThreadedTask *> tasks) {
	_general_thread_pool.enqueue(tasks);
}

void VoxelServer::push_async_io_task(zylann::IThreadedTask *task) {
	_streaming_thread_pool.enqueue(task);
}

void VoxelServer::push_async_io_tasks(Span<zylann::IThreadedTask *> tasks) {
	_streaming_thread_pool.enqueue(tasks);
}

void VoxelServer::process() {
	ZN_PROFILE_SCOPE();
	ZN_PROFILE_PLOT("Static memory usage", int64_t(OS::get_singleton()->get_static_memory_usage()));
	ZN_PROFILE_PLOT("TimeSpread tasks", int64_t(_time_spread_task_runner.get_pending_count()));
	ZN_PROFILE_PLOT("Progressive tasks", int64_t(_progressive_task_runner.get_pending_count()));

	// Receive data updates
	_streaming_thread_pool.dequeue_completed_tasks([](zylann::IThreadedTask *task) {
		task->apply_result();
		memdelete(task);
	});

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

static VoxelServer::Stats::ThreadPoolStats debug_get_pool_stats(const zylann::ThreadedTaskRunner &pool) {
	VoxelServer::Stats::ThreadPoolStats d;
	d.tasks = pool.get_debug_remaining_tasks();
	d.active_threads = debug_get_active_thread_count(pool);
	d.thread_count = pool.get_thread_count();
	return d;
}

Dictionary VoxelServer::Stats::to_dict() {
	Dictionary pools;
	pools["streaming"] = streaming.to_dict();
	pools["general"] = general.to_dict();

	Dictionary tasks;
	tasks["streaming"] = streaming_tasks;
	tasks["generation"] = generation_tasks;
	tasks["meshing"] = meshing_tasks;
	tasks["main_thread"] = main_thread_tasks;

	// This part is additional for scripts because VoxelMemoryPool is not exposed
	Dictionary mem;
	mem["voxel_total"] = ZN_SIZE_T_TO_VARIANT(VoxelMemoryPool::get_singleton().debug_get_total_memory());
	mem["voxel_used"] = ZN_SIZE_T_TO_VARIANT(VoxelMemoryPool::get_singleton().debug_get_used_memory());
	mem["block_count"] = VoxelMemoryPool::get_singleton().debug_get_used_blocks();

	Dictionary d;
	d["thread_pools"] = pools;
	d["tasks"] = tasks;
	d["memory_pools"] = mem;
	return d;
}

VoxelServer::Stats VoxelServer::get_stats() const {
	Stats s;
	s.streaming = debug_get_pool_stats(_streaming_thread_pool);
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
