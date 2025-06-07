#include "voxel_engine.h"
#include "../constants/voxel_constants.h"
#include "../generators/generate_block_task.h"
#include "../meshers/mesh_block_task.h"
#include "../streams/load_all_blocks_data_task.h"
#include "../streams/load_block_data_task.h"
#include "../streams/save_block_data_task.h"
#include "../util/godot/classes/os.h"
#include "../util/godot/classes/project_settings.h"
#include "../util/godot/classes/rd_sampler_state.h"
#include "../util/godot/classes/rendering_device.h"
#include "../util/godot/classes/rendering_server.h"
#include "../util/io/log.h"
#include "../util/macros.h"
#include "../util/math/conv.h"
#include "../util/profiling.h"
#include "../util/string/format.h"

namespace zylann::voxel {

VoxelEngine *g_voxel_engine = nullptr;

VoxelEngine &VoxelEngine::get_singleton() {
	ZN_ASSERT_MSG(g_voxel_engine != nullptr, "Accessing singleton while it's null");
	return *g_voxel_engine;
}

void VoxelEngine::create_singleton(Config config) {
	ZN_ASSERT_MSG(g_voxel_engine == nullptr, "Creating singleton twice");
	g_voxel_engine = ZN_NEW(VoxelEngine(config));
}

void VoxelEngine::destroy_singleton() {
	ZN_ASSERT_MSG(g_voxel_engine != nullptr, "Destroying singleton twice");
	ZN_DELETE(g_voxel_engine);
	g_voxel_engine = nullptr;
}

VoxelEngine::VoxelEngine(Config config) {
	const int hw_threads_hint = Thread::get_hardware_concurrency();
	ZN_PRINT_VERBOSE(format("Voxel: HW threads hint: {}", hw_threads_hint));

	ZN_ASSERT(config.thread_count_margin_below_max >= 0);
	ZN_ASSERT(config.thread_count_minimum >= 1);
	ZN_ASSERT(config.thread_count_ratio_over_max >= 0.f);

	// Compute thread count for general pool.
	// Note that the I/O thread counts as one used thread and will always be present.

	const int maximum_thread_count =
			math::max(hw_threads_hint - config.thread_count_margin_below_max, config.thread_count_minimum);
	const int thread_count_by_ratio = int(Math::round(float(config.thread_count_ratio_over_max) * hw_threads_hint));
	const int thread_count = math::clamp(thread_count_by_ratio, config.thread_count_minimum, maximum_thread_count);
	ZN_PRINT_VERBOSE(format("Voxel: automatic thread count set to {}", thread_count));

	if (thread_count > hw_threads_hint) {
		ZN_PRINT_WARNING("Configured thread count exceeds hardware thread count. Performance may not be optimal");
	}

	_general_thread_pool.set_name("Voxel general");
	_general_thread_pool.set_thread_count(thread_count);
	_general_thread_pool.set_priority_update_period(200);

	// Init world
	_world.shared_priority_dependency = make_shared_instance<PriorityDependency::ViewersData>();
	// Give initial capacity to make invalidation less likely
	_world.shared_priority_dependency->viewers.resize(64);

	ZN_PRINT_VERBOSE(format("Size of LoadBlockDataTask: {}", sizeof(LoadBlockDataTask)));
	ZN_PRINT_VERBOSE(format("Size of SaveBlockDataTask: {}", sizeof(SaveBlockDataTask)));
	ZN_PRINT_VERBOSE(format("Size of MeshBlockTask: {}", sizeof(MeshBlockTask)));

	set_main_thread_time_budget_usec(config.main_thread_budget_usec);
}

VoxelEngine::~VoxelEngine() {
	// The GDScriptLanguage singleton can get destroyed before ours, so any script referenced by tasks
	// cannot be freed. To work this around, tasks are cleared when the scene tree autoload is destroyed.
	// So normally there should not be any task left to clear here,
	// but doing it anyways for correctness, it's how it should have been...
	// See https://github.com/Zylann/godot_voxel/issues/189
	wait_and_clear_all_tasks(true);

#ifdef VOXEL_ENABLE_GPU
	_gpu_task_runner.stop();
#endif
}

static bool auto_detect_threaded_graphics_resource_building_support() {
	const ProjectSettings *project = ProjectSettings::get_singleton();
	ZN_ASSERT_RETURN_V(project != nullptr, false);

	const zylann::godot::RenderThreadModel rendering_thread_model = zylann::godot::get_render_thread_model(*project);
	const zylann::godot::RenderMethod rendering_method = zylann::godot::get_current_rendering_method();
	const zylann::godot::RenderDriverName driver = zylann ::godot::get_current_rendering_driver();

	if (!zylann::godot::is_render_thread_model_safe(rendering_thread_model)) {
		return false;
	}

	switch (rendering_method) {
		case zylann::godot::RENDER_METHOD_GL_COMPATIBILITY:
			return false;
		case zylann::godot::RENDER_METHOD_UNKNOWN:
			return false;
		default:
			break;
	}

	switch (driver) {
		case zylann::godot::RENDER_DRIVER_OPENGL3:
		case zylann::godot::RENDER_DRIVER_OPENGL3_ANGLE:
		case zylann::godot::RENDER_DRIVER_OPENGL3_ES:
		case zylann::godot::RENDER_DRIVER_UNKNOWN:
			return false;
		default:
			return true;
	}
}

void VoxelEngine::try_initialize_gpu_features() {
	_threaded_graphics_resource_building_enabled = auto_detect_threaded_graphics_resource_building_support();
	ZN_PRINT_VERBOSE(format(
			"Auto-detected threaded graphics resource building: {}", _threaded_graphics_resource_building_enabled
	));

#ifdef VOXEL_ENABLE_GPU
	if (_gpu_task_runner.is_running() == false) {
		_gpu_task_runner.start();
	}
#endif
}

void VoxelEngine::wait_and_clear_all_tasks(bool warn) {
	_general_thread_pool.wait_for_all_tasks();

	_general_thread_pool.dequeue_completed_tasks([warn](zylann::IThreadedTask *task) {
		if (warn) {
			ZN_PRINT_WARNING(
					"General tasks remain on module cleanup, "
					"this could become a problem if they reference scripts"
			);
		}
		ZN_DELETE(task);
	});
}

VolumeID VoxelEngine::add_volume(VolumeCallbacks callbacks) {
	ZN_ASSERT(callbacks.check_callbacks());
	Volume volume;
	volume.callbacks = callbacks;
	return _world.volumes.add(volume);
}

VoxelEngine::VolumeCallbacks VoxelEngine::get_volume_callbacks(VolumeID volume_id) const {
	const Volume &volume = _world.volumes.get(volume_id);
	return volume.callbacks;
}

void VoxelEngine::remove_volume(VolumeID volume_id) {
	_world.volumes.remove(volume_id);
	// TODO How to cancel meshing tasks?

	if (_world.volumes.count() == 0) {
		// To workaround https://github.com/Zylann/godot_voxel/issues/189
		// When the last remaining volume got destroyed (as in game exit)
		wait_and_clear_all_tasks(false);
	}
}

bool VoxelEngine::is_volume_valid(VolumeID volume_id) const {
	return _world.volumes.exists(volume_id);
}

ViewerID VoxelEngine::add_viewer() {
	return _world.viewers.add(Viewer());
}

void VoxelEngine::remove_viewer(ViewerID viewer_id) {
	_world.viewers.remove(viewer_id);
}

bool VoxelEngine::get_viewer_count() const {
	return _world.viewers.count();
}

void VoxelEngine::set_viewer_position(ViewerID viewer_id, Vector3 position) {
	Viewer &viewer = _world.viewers.get(viewer_id);
	viewer.world_position = position;
}

void VoxelEngine::set_viewer_distances(ViewerID viewer_id, Viewer::Distances distances) {
	Viewer &viewer = _world.viewers.get(viewer_id);
	viewer.view_distances = distances;
}

VoxelEngine::Viewer::Distances VoxelEngine::get_viewer_distances(ViewerID viewer_id) const {
	const Viewer &viewer = _world.viewers.get(viewer_id);
	return viewer.view_distances;
}

void VoxelEngine::set_viewer_requires_visuals(ViewerID viewer_id, bool enabled) {
	Viewer &viewer = _world.viewers.get(viewer_id);
	viewer.require_visuals = enabled;
}

bool VoxelEngine::is_viewer_requiring_visuals(ViewerID viewer_id) const {
	const Viewer &viewer = _world.viewers.get(viewer_id);
	return viewer.require_visuals;
}

void VoxelEngine::set_viewer_requires_collisions(ViewerID viewer_id, bool enabled) {
	Viewer &viewer = _world.viewers.get(viewer_id);
	viewer.require_collisions = enabled;
}

bool VoxelEngine::is_viewer_requiring_collisions(ViewerID viewer_id) const {
	const Viewer &viewer = _world.viewers.get(viewer_id);
	return viewer.require_collisions;
}

void VoxelEngine::set_viewer_requires_data_block_notifications(ViewerID viewer_id, bool enabled) {
	Viewer &viewer = _world.viewers.get(viewer_id);
	viewer.requires_data_block_notifications = enabled;
}

bool VoxelEngine::is_viewer_requiring_data_block_notifications(ViewerID viewer_id) const {
	const Viewer &viewer = _world.viewers.get(viewer_id);
	return viewer.requires_data_block_notifications;
}

void VoxelEngine::set_viewer_network_peer_id(ViewerID viewer_id, int peer_id) {
	Viewer &viewer = _world.viewers.get(viewer_id);
	viewer.network_peer_id = peer_id;
}

int VoxelEngine::get_viewer_network_peer_id(ViewerID viewer_id) const {
	const Viewer &viewer = _world.viewers.get(viewer_id);
	return viewer.network_peer_id;
}

bool VoxelEngine::viewer_exists(ViewerID viewer_id) const {
	return _world.viewers.exists(viewer_id);
}

void VoxelEngine::push_main_thread_time_spread_task(
		zylann::ITimeSpreadTask *task,
		TimeSpreadTaskRunner::Priority priority
) {
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

bool VoxelEngine::is_threaded_graphics_resource_building_enabled() const {
	return _threaded_graphics_resource_building_enabled;
}

// void VoxelEngine::set_threaded_graphics_resource_building_enabled(bool enabled) {
// 	_threaded_graphics_resource_building_enabled = enabled;
// }

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

#ifdef VOXEL_ENABLE_GPU
void VoxelEngine::push_gpu_task(IGPUTask *task) {
	_gpu_task_runner.push(task);
}

uint32_t VoxelEngine::get_pending_gpu_tasks_count() const {
	return _gpu_task_runner.get_pending_task_count();
}
#endif

void VoxelEngine::process() {
	ZN_PROFILE_SCOPE();
	ZN_PROFILE_PLOT("Static memory usage", int64_t(OS::get_singleton()->get_static_memory_usage()));
	ZN_PROFILE_PLOT("TimeSpread tasks", int64_t(_time_spread_task_runner.get_pending_count()));
	ZN_PROFILE_PLOT("Progressive tasks", int64_t(_progressive_task_runner.get_pending_count()));
	ZN_PROFILE_PLOT("Threaded tasks", int64_t(_general_thread_pool.get_debug_remaining_tasks()));
	ZN_PROFILE_PLOT("Objects", int64_t(ObjectDB::get_object_count()));
	ZN_PROFILE_PLOT(
			"ZN Std Allocator",
			int64_t(StdDefaultAllocatorCounters::g_allocated - StdDefaultAllocatorCounters::g_deallocated)
	);

	// Receive generation and meshing results
	_general_thread_pool.dequeue_completed_tasks([](zylann::IThreadedTask *task) {
		task->apply_result();
		ZN_DELETE(task);
	});

	// Run this after dequeueing threaded tasks, because they can add some to this runner,
	// which could in turn complete right away (we avoid 1-frame delays this way).
	_time_spread_task_runner.process(_main_thread_time_budget_usec);

	_progressive_task_runner.process();

	// Update viewer dependencies
	sync_viewers_task_priority_data();

#ifdef VOXEL_ENABLE_GPU
	ZN_PROFILE_PLOT("Pending GPU tasks", int64_t(_gpu_task_runner.get_pending_task_count()));
#endif
}

void VoxelEngine::sync_viewers_task_priority_data() {
	const unsigned int viewer_count = _world.viewers.count();

	if (viewer_count > _world.shared_priority_dependency->viewers.size()) {
		// Invalidate, build a new one. Will be referenced by next tasks onwards.

		// One edge case of this is when there is a stockpile of existing tasks lasting for a long time (for example if
		// the user has an extremely slow generator). Priority of those tasks will no longer be updated dynamically, so
		// it's possible that some chunks will load at an odd pace. To workaround this, we can minimize the times this
		// invalidation occurs by preallocating enough elements. Exceeding this capacity will make the issue come back,
		// but it should be rare. Eventually, if a game requires using a lot of viewers, we may find a different
		// strategy that doesn't involve iterating them all?

		// TODO We can avoid the invalidation by using an atomic size or memory barrier?
		_world.shared_priority_dependency = make_shared_instance<PriorityDependency::ViewersData>();
		_world.shared_priority_dependency->viewers.resize(viewer_count);
	}

	PriorityDependency::ViewersData &dep = *_world.shared_priority_dependency;

	size_t i = 0;
	unsigned int max_distance = 0;
	_world.viewers.for_each_value([&i, &max_distance, &dep](Viewer &viewer) {
		dep.viewers[i] = to_vec3f(viewer.world_position);
		max_distance = math::max(max_distance, viewer.view_distances.max());
		++i;
	});

	dep.viewers_count = viewer_count;

	// Cancel distance is increased because of two reasons:
	// - Some volumes use a cubic area which has higher distances on their corners
	// - Hysteresis is needed to reduce ping-pong
	dep.highest_view_distance = max_distance * 2;
}

namespace {

unsigned int debug_get_active_thread_count(const zylann::ThreadedTaskRunner &pool) {
	unsigned int active_count = 0;
	for (unsigned int i = 0; i < pool.get_thread_count(); ++i) {
		zylann::ThreadedTaskRunner::State s = pool.get_thread_debug_state(i);
		if (s == zylann::ThreadedTaskRunner::STATE_RUNNING) {
			++active_count;
		}
	}
	return active_count;
}

VoxelEngine::Stats::ThreadPoolStats debug_get_pool_stats(const zylann::ThreadedTaskRunner &pool) {
	VoxelEngine::Stats::ThreadPoolStats d;
	d.tasks = pool.get_debug_remaining_tasks();
	d.active_threads = debug_get_active_thread_count(pool);
	d.thread_count = pool.get_thread_count();

	fill(d.active_task_names, (const char *)nullptr);
	for (unsigned int i = 0; i < d.thread_count; ++i) {
		d.active_task_names[i] = pool.get_thread_debug_task_name(i);
	}

	return d;
}

} // namespace

VoxelEngine::Stats VoxelEngine::get_stats() const {
	Stats s;
	s.general = debug_get_pool_stats(_general_thread_pool);
	s.generation_tasks = _debug_generate_block_task_count;
	s.meshing_tasks = MeshBlockTask::debug_get_running_count();
	s.streaming_tasks = LoadBlockDataTask::debug_get_running_count() + SaveBlockDataTask::debug_get_running_count();
	s.main_thread_tasks = _time_spread_task_runner.get_pending_count() + _progressive_task_runner.get_pending_count();
#ifdef VOXEL_ENABLE_GPU
	s.gpu_tasks = _gpu_task_runner.get_pending_task_count();
#endif
	return s;
}

} // namespace zylann::voxel
