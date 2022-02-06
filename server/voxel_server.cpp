#include "voxel_server.h"
#include "../constants/voxel_constants.h"
#include "../storage/voxel_memory_pool.h"
#include "../util/funcs.h"
#include "../util/godot/funcs.h"
#include "../util/macros.h"
#include "../util/profiling.h"
#include "generate_block_task.h"
#include "load_all_blocks_data_task.h"
#include "load_block_data_task.h"
#include "save_block_data_task.h"

#include <core/config/project_settings.h>
#include <core/os/memory.h>
#include <thread>

namespace zylann::voxel {

VoxelServer *g_voxel_server = nullptr;

VoxelServer *VoxelServer::get_singleton() {
	CRASH_COND_MSG(g_voxel_server == nullptr, "Accessing singleton while it's null");
	return g_voxel_server;
}

void VoxelServer::create_singleton() {
	CRASH_COND_MSG(g_voxel_server != nullptr, "Creating singleton twice");
	g_voxel_server = memnew(VoxelServer);
}

void VoxelServer::destroy_singleton() {
	CRASH_COND_MSG(g_voxel_server == nullptr, "Destroying singleton twice");
	memdelete(g_voxel_server);
	g_voxel_server = nullptr;
}

VoxelServer::VoxelServer() {
	CRASH_COND(ProjectSettings::get_singleton() == nullptr);

	const int hw_threads_hint = std::thread::hardware_concurrency();
	PRINT_VERBOSE(String("Voxel: HW threads hint: {0}").format(varray(hw_threads_hint)));

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
	PRINT_VERBOSE(String("Voxel: automatic thread count set to {0}").format(varray(thread_count)));

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
	_world.shared_priority_dependency = gd_make_shared<PriorityDependency::ViewersData>();

	PRINT_VERBOSE(String("Size of LoadBlockDataTask: {0}").format(varray((int)sizeof(LoadBlockDataTask))));
	PRINT_VERBOSE(String("Size of SaveBlockDataTask: {0}").format(varray((int)sizeof(SaveBlockDataTask))));
	PRINT_VERBOSE(String("Size of MeshBlockTask: {0}").format(varray((int)sizeof(MeshBlockTask))));
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
	volume.meshing_dependency = gd_make_shared<MeshBlockTask::MeshingDependency>();
	return _world.volumes.create(volume);
}

void VoxelServer::set_volume_transform(uint32_t volume_id, Transform3D t) {
	Volume &volume = _world.volumes.get(volume_id);
	volume.transform = t;
}

void VoxelServer::set_volume_render_block_size(uint32_t volume_id, uint32_t block_size) {
	Volume &volume = _world.volumes.get(volume_id);
	volume.render_block_size = block_size;
}

void VoxelServer::set_volume_data_block_size(uint32_t volume_id, uint32_t block_size) {
	Volume &volume = _world.volumes.get(volume_id);
	volume.data_block_size = block_size;
}

void VoxelServer::set_volume_stream(uint32_t volume_id, Ref<VoxelStream> stream) {
	Volume &volume = _world.volumes.get(volume_id);
	volume.stream = stream;

	// Commit a new dependency to process requests with
	if (volume.stream_dependency != nullptr) {
		volume.stream_dependency->valid = false;
	}

	volume.stream_dependency = gd_make_shared<StreamingDependency>();
	volume.stream_dependency->generator = volume.generator;
	volume.stream_dependency->stream = volume.stream;
}

void VoxelServer::set_volume_generator(uint32_t volume_id, Ref<VoxelGenerator> generator) {
	Volume &volume = _world.volumes.get(volume_id);
	volume.generator = generator;

	// Commit a new dependency to process requests with
	if (volume.stream_dependency != nullptr) {
		volume.stream_dependency->valid = false;
	}

	volume.stream_dependency = gd_make_shared<StreamingDependency>();
	volume.stream_dependency->generator = volume.generator;
	volume.stream_dependency->stream = volume.stream;

	if (volume.meshing_dependency != nullptr) {
		volume.meshing_dependency->valid = false;
	}

	volume.meshing_dependency = gd_make_shared<MeshBlockTask::MeshingDependency>();
	volume.meshing_dependency->mesher = volume.mesher;
	volume.meshing_dependency->generator = volume.generator;
}

void VoxelServer::set_volume_mesher(uint32_t volume_id, Ref<VoxelMesher> mesher) {
	Volume &volume = _world.volumes.get(volume_id);
	volume.mesher = mesher;

	if (volume.meshing_dependency != nullptr) {
		volume.meshing_dependency->valid = false;
	}

	volume.meshing_dependency = gd_make_shared<MeshBlockTask::MeshingDependency>();
	volume.meshing_dependency->mesher = volume.mesher;
	volume.meshing_dependency->generator = volume.generator;
}

void VoxelServer::set_volume_octree_lod_distance(uint32_t volume_id, float lod_distance) {
	Volume &volume = _world.volumes.get(volume_id);
	volume.octree_lod_distance = lod_distance;
}

void VoxelServer::invalidate_volume_mesh_requests(uint32_t volume_id) {
	Volume &volume = _world.volumes.get(volume_id);
	volume.meshing_dependency->valid = false;
	volume.meshing_dependency = gd_make_shared<MeshBlockTask::MeshingDependency>();
	volume.meshing_dependency->mesher = volume.mesher;
	volume.meshing_dependency->generator = volume.generator;
}

VoxelServer::VolumeCallbacks VoxelServer::get_volume_callbacks(uint32_t volume_id) const {
	const Volume &volume = _world.volumes.get(volume_id);
	return volume.callbacks;
}

static inline Vector3i get_block_center(Vector3i pos, int bs, int lod) {
	return (pos << lod) * bs + Vector3iUtil::create(bs / 2);
}

void VoxelServer::init_priority_dependency(
		PriorityDependency &dep, Vector3i block_position, uint8_t lod, const Volume &volume, int block_size) {
	const Vector3i voxel_pos = get_block_center(block_position, block_size, lod);
	const float block_radius = (block_size << lod) / 2;
	dep.shared = _world.shared_priority_dependency;
	dep.world_position = volume.transform.xform(voxel_pos);
	const float transformed_block_radius =
			volume.transform.basis.xform(Vector3(block_radius, block_radius, block_radius)).length();

	switch (volume.type) {
		case VOLUME_SPARSE_GRID:
			// Distance beyond which no field of view can overlap the block.
			// Doubling block radius to account for an extra margin of blocks,
			// since they are used to provide neighbors when meshing
			dep.drop_distance_squared = math::squared(
					_world.shared_priority_dependency->highest_view_distance + 2.f * transformed_block_radius);
			break;

		case VOLUME_SPARSE_OCTREE:
			// Distance beyond which it is safe to drop a block without risking to block LOD subdivision.
			// This does not depend on viewer's view distance, but on LOD precision instead.
			dep.drop_distance_squared = math::squared(2.f * transformed_block_radius *
					get_octree_lod_block_region_extent(volume.octree_lod_distance, block_size));
			break;

		default:
			CRASH_NOW_MSG("Unexpected type");
			break;
	}
}

void VoxelServer::request_block_mesh(uint32_t volume_id, const BlockMeshInput &input) {
	const Volume &volume = _world.volumes.get(volume_id);
	ERR_FAIL_COND(volume.meshing_dependency == nullptr);
	ERR_FAIL_COND(volume.meshing_dependency->mesher.is_null());
	ERR_FAIL_COND(volume.data_block_size > 255);

	MeshBlockTask *task = memnew(MeshBlockTask);
	task->volume_id = volume_id;
	task->blocks = input.data_blocks;
	task->blocks_count = input.data_blocks_count;
	task->position = input.render_block_position;
	task->lod = input.lod;
	task->meshing_dependency = volume.meshing_dependency;
	task->data_block_size = volume.data_block_size;

	init_priority_dependency(
			task->priority_dependency, input.render_block_position, input.lod, volume, volume.render_block_size);

	// We'll allocate this quite often. If it becomes a problem, it should be easy to pool.
	_general_thread_pool.enqueue(task);
}

void VoxelServer::request_block_load(uint32_t volume_id, Vector3i block_pos, int lod, bool request_instances) {
	const Volume &volume = _world.volumes.get(volume_id);
	ERR_FAIL_COND(volume.stream_dependency == nullptr);
	ERR_FAIL_COND(volume.data_block_size > 255);

	if (volume.stream_dependency->stream.is_valid()) {
		PriorityDependency priority_dependency;
		init_priority_dependency(priority_dependency, block_pos, lod, volume, volume.data_block_size);

		LoadBlockDataTask *task = memnew(LoadBlockDataTask(volume_id, block_pos, lod, volume.data_block_size,
				request_instances, volume.stream_dependency, priority_dependency));

		_streaming_thread_pool.enqueue(task);

	} else {
		// Directly generate the block without checking the stream
		ERR_FAIL_COND(volume.stream_dependency->generator.is_null());

		GenerateBlockTask *task = memnew(GenerateBlockTask);
		task->volume_id = volume_id;
		task->position = block_pos;
		task->lod = lod;
		task->block_size = volume.data_block_size;
		task->stream_dependency = volume.stream_dependency;

		init_priority_dependency(task->priority_dependency, block_pos, lod, volume, volume.data_block_size);

		_general_thread_pool.enqueue(task);
	}
}

void VoxelServer::request_block_generate(
		uint32_t volume_id, Vector3i block_pos, int lod, std::shared_ptr<zylann::AsyncDependencyTracker> tracker) {
	//
	const Volume &volume = _world.volumes.get(volume_id);
	ERR_FAIL_COND(volume.stream_dependency->generator.is_null());

	GenerateBlockTask *task = memnew(GenerateBlockTask);
	task->volume_id = volume_id;
	task->position = block_pos;
	task->lod = lod;
	task->block_size = volume.data_block_size;
	task->stream_dependency = volume.stream_dependency;
	task->tracker = tracker;
	task->drop_beyond_max_distance = false;

	init_priority_dependency(task->priority_dependency, block_pos, lod, volume, volume.data_block_size);

	_general_thread_pool.enqueue(task);
}

void VoxelServer::request_all_stream_blocks(uint32_t volume_id) {
	PRINT_VERBOSE(String("Request all blocks for volume {0}").format(varray(volume_id)));
	const Volume &volume = _world.volumes.get(volume_id);
	ERR_FAIL_COND(volume.stream.is_null());
	CRASH_COND(volume.stream_dependency == nullptr);

	LoadAllBlocksDataTask *task = memnew(LoadAllBlocksDataTask);
	task->volume_id = volume_id;
	task->stream_dependency = volume.stream_dependency;

	_general_thread_pool.enqueue(task);
}

void VoxelServer::request_voxel_block_save(
		uint32_t volume_id, std::shared_ptr<VoxelBufferInternal> voxels, Vector3i block_pos, int lod) {
	//
	const Volume &volume = _world.volumes.get(volume_id);
	ERR_FAIL_COND(volume.stream.is_null());
	CRASH_COND(volume.stream_dependency == nullptr);

	SaveBlockDataTask *task = memnew(
			SaveBlockDataTask(volume_id, block_pos, lod, volume.data_block_size, voxels, volume.stream_dependency));

	// No priority data, saving doesnt need sorting

	_streaming_thread_pool.enqueue(task);
}

void VoxelServer::request_instance_block_save(
		uint32_t volume_id, std::unique_ptr<InstanceBlockData> instances, Vector3i block_pos, int lod) {
	const Volume &volume = _world.volumes.get(volume_id);
	ERR_FAIL_COND(volume.stream.is_null());
	CRASH_COND(volume.stream_dependency == nullptr);

	SaveBlockDataTask *task = memnew(SaveBlockDataTask(
			volume_id, block_pos, lod, volume.data_block_size, std::move(instances), volume.stream_dependency));

	// No priority data, saving doesnt need sorting

	_streaming_thread_pool.enqueue(task);
}

void VoxelServer::remove_volume(uint32_t volume_id) {
	{
		Volume &volume = _world.volumes.get(volume_id);
		if (volume.stream_dependency != nullptr) {
			volume.stream_dependency->valid = false;
		}
		if (volume.meshing_dependency != nullptr) {
			volume.meshing_dependency->valid = false;
		}
	}

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

void VoxelServer::push_time_spread_task(zylann::ITimeSpreadTask *task) {
	_time_spread_task_runner.push(task);
}

void VoxelServer::push_progressive_task(zylann::IProgressiveTask *task) {
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

void VoxelServer::process() {
	VOXEL_PROFILE_SCOPE();
	VOXEL_PROFILE_PLOT("Static memory usage", int64_t(OS::get_singleton()->get_static_memory_usage()));
	VOXEL_PROFILE_PLOT("TimeSpread tasks", int64_t(_time_spread_task_runner.get_pending_count()));
	VOXEL_PROFILE_PLOT("Progressive tasks", int64_t(_progressive_task_runner.get_pending_count()));

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
			_world.shared_priority_dependency = gd_make_shared<PriorityDependency::ViewersData>();
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
	mem["voxel_total"] = SIZE_T_TO_VARIANT(VoxelMemoryPool::get_singleton()->debug_get_total_memory());
	mem["voxel_used"] = SIZE_T_TO_VARIANT(VoxelMemoryPool::get_singleton()->debug_get_used_memory());
	mem["block_count"] = VoxelMemoryPool::get_singleton()->debug_get_used_blocks();

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

} // namespace zylann::voxel
