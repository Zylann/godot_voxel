#include "voxel_server.h"
#include "../constants/voxel_constants.h"
#include "../storage/voxel_memory_pool.h"
#include "../util/funcs.h"
#include "../util/godot/funcs.h"
#include "../util/macros.h"
#include "../util/profiling.h"

#include <core/os/memory.h>
#include <scene/main/viewport.h>
#include <thread>

namespace {
VoxelServer *g_voxel_server = nullptr;
// Could be atomics, but it's for debugging so I don't bother for now
int g_debug_generate_tasks_count = 0;
int g_debug_stream_tasks_count = 0;
int g_debug_mesh_tasks_count = 0;
} // namespace

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

VoxelTimeSpreadTaskRunner::~VoxelTimeSpreadTaskRunner() {
	flush();
}

void VoxelTimeSpreadTaskRunner::push(IVoxelTimeSpreadTask *task) {
	_tasks.push(task);
}

void VoxelTimeSpreadTaskRunner::process(uint64_t time_budget_usec) {
	VOXEL_PROFILE_SCOPE();
	const OS &os = *OS::get_singleton();

	if (_tasks.size() > 0) {
		const uint64_t time_before = os.get_ticks_usec();

		// Do at least one task
		do {
			IVoxelTimeSpreadTask *task = _tasks.front();
			_tasks.pop();
			task->run();
			// TODO Call recycling function instead?
			memdelete(task);

		} while (_tasks.size() > 0 && os.get_ticks_usec() - time_before < time_budget_usec);
	}
}

void VoxelTimeSpreadTaskRunner::flush() {
	while (!_tasks.empty()) {
		IVoxelTimeSpreadTask *task = _tasks.front();
		_tasks.pop();
		task->run();
		memdelete(task);
	}
}

unsigned int VoxelTimeSpreadTaskRunner::get_pending_count() const {
	return _tasks.size();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
			PropertyInfo(Variant::REAL, "voxel/threads/count/ratio_over_max", PROPERTY_HINT_RANGE, "0,1,0.1"));

	GLOBAL_DEF_RST("voxel/threads/main/time_budget_ms", 8);
	ProjectSettings::get_singleton()->set_custom_property_info("voxel/threads/main/time_budget_ms",
			PropertyInfo(Variant::INT, "voxel/threads/main/time_budget_ms", PROPERTY_HINT_RANGE, "0,1000"));

	_main_thread_time_budget_usec =
			1000 * int(ProjectSettings::get_singleton()->get("voxel/threads/main/time_budget_ms"));

	const int minimum_thread_count = max(1, int(ProjectSettings::get_singleton()->get("voxel/threads/count/minimum")));

	// How many threads below available count on the CPU should we set as limit
	const int thread_count_margin =
			max(1, int(ProjectSettings::get_singleton()->get("voxel/threads/count/margin_below_max")));

	// Portion of available CPU threads to attempt using
	const float threads_ratio =
			clamp(float(ProjectSettings::get_singleton()->get("voxel/threads/count/ratio_over_max")), 0.f, 1.f);

	const int maximum_thread_count = max(hw_threads_hint - thread_count_margin, minimum_thread_count);
	// `-1` is for the stream thread
	const int thread_count_by_ratio = int(Math::round(float(threads_ratio) * hw_threads_hint)) - 1;
	const int thread_count = clamp(thread_count_by_ratio, minimum_thread_count, maximum_thread_count);
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
	_world.shared_priority_dependency = gd_make_shared<PriorityDependencyShared>();

	PRINT_VERBOSE(String("Size of BlockDataRequest: {0}").format(varray((int)sizeof(BlockDataRequest))));
	PRINT_VERBOSE(String("Size of BlockMeshRequest: {0}").format(varray((int)sizeof(BlockMeshRequest))));
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

	_streaming_thread_pool.dequeue_completed_tasks([warn](IVoxelTask *task) {
		if (warn) {
			WARN_PRINT("Streaming tasks remain on module cleanup, "
					   "this could become a problem if they reference scripts");
		}
		memdelete(task);
	});

	_general_thread_pool.dequeue_completed_tasks([warn](IVoxelTask *task) {
		if (warn) {
			WARN_PRINT("General tasks remain on module cleanup, "
					   "this could become a problem if they reference scripts");
		}
		memdelete(task);
	});
}

int VoxelServer::get_priority(const PriorityDependency &dep, uint8_t lod_index, float *out_closest_distance_sq) {
	const std::vector<Vector3> &viewer_positions = dep.shared->viewers;
	const Vector3 block_position = dep.world_position;

	float closest_distance_sq = 99999.f;
	if (viewer_positions.size() == 0) {
		// Assume origin
		closest_distance_sq = block_position.length_squared();
	} else {
		for (size_t i = 0; i < viewer_positions.size(); ++i) {
			float d = viewer_positions[i].distance_squared_to(block_position);
			if (d < closest_distance_sq) {
				closest_distance_sq = d;
			}
		}
	}

	if (out_closest_distance_sq != nullptr) {
		*out_closest_distance_sq = closest_distance_sq;
	}

	// TODO Any way to optimize out the sqrt?
	// I added it because the LOD modifier was not working with squared distances,
	// which led blocks to subdivide too much compared to their neighbors, making cracks more likely to happen
	int priority = static_cast<int>(Math::sqrt(closest_distance_sq));

	// TODO Prioritizing LOD makes generation slower... but not prioritizing makes cracks more likely to appear...
	// This could be fixed by allowing the volume to preemptively request blocks of the next LOD?
	//
	// Higher lod indexes come first to allow the octree to subdivide.
	// Then comes distance, which is modified by how much in view the block is
	priority += (VoxelConstants::MAX_LOD - lod_index) * 10000;

	return priority;
}

uint32_t VoxelServer::add_volume(ReceptionBuffers *buffers, VolumeType type) {
	CRASH_COND(buffers == nullptr);
	Volume volume;
	volume.type = type;
	volume.reception_buffers = buffers;
	volume.meshing_dependency = gd_make_shared<MeshingDependency>();
	return _world.volumes.create(volume);
}

void VoxelServer::set_volume_transform(uint32_t volume_id, Transform t) {
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
}

void VoxelServer::set_volume_mesher(uint32_t volume_id, Ref<VoxelMesher> mesher) {
	Volume &volume = _world.volumes.get(volume_id);
	volume.mesher = mesher;
	volume.meshing_dependency = gd_make_shared<MeshingDependency>();
	volume.meshing_dependency->mesher = volume.mesher;
}

void VoxelServer::set_volume_octree_lod_distance(uint32_t volume_id, float lod_distance) {
	Volume &volume = _world.volumes.get(volume_id);
	volume.octree_lod_distance = lod_distance;
}

void VoxelServer::invalidate_volume_mesh_requests(uint32_t volume_id) {
	Volume &volume = _world.volumes.get(volume_id);
	volume.meshing_dependency->valid = false;
	volume.meshing_dependency = gd_make_shared<MeshingDependency>();
	volume.meshing_dependency->mesher = volume.mesher;
}

static inline Vector3i get_block_center(Vector3i pos, int bs, int lod) {
	return (pos << lod) * bs + Vector3i(bs / 2);
}

void VoxelServer::init_priority_dependency(
		VoxelServer::PriorityDependency &dep, Vector3i block_position, uint8_t lod, const Volume &volume, int block_size) {
	const Vector3i voxel_pos = get_block_center(block_position, block_size, lod);
	const float block_radius = (block_size << lod) / 2;
	dep.shared = _world.shared_priority_dependency;
	dep.world_position = volume.transform.xform(voxel_pos.to_vec3());
	const float transformed_block_radius =
			volume.transform.basis.xform(Vector3(block_radius, block_radius, block_radius)).length();

	switch (volume.type) {
		case VOLUME_SPARSE_GRID:
			// Distance beyond which no field of view can overlap the block.
			// Doubling block radius to account for an extra margin of blocks,
			// since they are used to provide neighbors when meshing
			dep.drop_distance_squared =
					squared(_world.shared_priority_dependency->highest_view_distance + 2.f * transformed_block_radius);
			break;

		case VOLUME_SPARSE_OCTREE:
			// Distance beyond which it is safe to drop a block without risking to block LOD subdivision.
			// This does not depend on viewer's view distance, but on LOD precision instead.
			dep.drop_distance_squared =
					squared(2.f * transformed_block_radius *
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

	BlockMeshRequest *r = memnew(BlockMeshRequest);
	r->volume_id = volume_id;
	r->blocks = input.data_blocks;
	r->blocks_count = input.data_blocks_count;
	r->position = input.render_block_position;
	r->lod = input.lod;
	r->meshing_dependency = volume.meshing_dependency;

	init_priority_dependency(
			r->priority_dependency, input.render_block_position, input.lod, volume, volume.render_block_size);

	// We'll allocate this quite often. If it becomes a problem, it should be easy to pool.
	_general_thread_pool.enqueue(r);
}

void VoxelServer::request_block_load(uint32_t volume_id, Vector3i block_pos, int lod, bool request_instances) {
	const Volume &volume = _world.volumes.get(volume_id);
	ERR_FAIL_COND(volume.stream_dependency == nullptr);

	if (volume.stream_dependency->stream.is_valid()) {
		BlockDataRequest *r = memnew(BlockDataRequest);
		r->volume_id = volume_id;
		r->position = block_pos;
		r->lod = lod;
		r->type = BlockDataRequest::TYPE_LOAD;
		r->block_size = volume.data_block_size;
		r->stream_dependency = volume.stream_dependency;
		r->request_instances = request_instances;

		init_priority_dependency(r->priority_dependency, block_pos, lod, volume, volume.data_block_size);

		_streaming_thread_pool.enqueue(r);

	} else {
		// Directly generate the block without checking the stream
		ERR_FAIL_COND(volume.stream_dependency->generator.is_null());

		BlockGenerateRequest *r = memnew(BlockGenerateRequest);
		r->volume_id = volume_id;
		r->position = block_pos;
		r->lod = lod;
		r->block_size = volume.data_block_size;
		r->stream_dependency = volume.stream_dependency;

		init_priority_dependency(r->priority_dependency, block_pos, lod, volume, volume.data_block_size);

		_general_thread_pool.enqueue(r);
	}
}

void VoxelServer::request_voxel_block_save(uint32_t volume_id, std::shared_ptr<VoxelBufferInternal> voxels,
		Vector3i block_pos, int lod) {
	//
	const Volume &volume = _world.volumes.get(volume_id);
	ERR_FAIL_COND(volume.stream.is_null());
	CRASH_COND(volume.stream_dependency == nullptr);

	BlockDataRequest *r = memnew(BlockDataRequest);
	r->voxels = voxels;
	r->volume_id = volume_id;
	r->position = block_pos;
	r->lod = lod;
	r->type = BlockDataRequest::TYPE_SAVE;
	r->block_size = volume.data_block_size;
	r->stream_dependency = volume.stream_dependency;
	r->request_instances = false;
	r->request_voxels = true;

	// No priority data, saving doesnt need sorting

	_streaming_thread_pool.enqueue(r);
}

void VoxelServer::request_instance_block_save(uint32_t volume_id, std::unique_ptr<VoxelInstanceBlockData> instances,
		Vector3i block_pos, int lod) {
	const Volume &volume = _world.volumes.get(volume_id);
	ERR_FAIL_COND(volume.stream.is_null());
	CRASH_COND(volume.stream_dependency == nullptr);

	BlockDataRequest *r = memnew(BlockDataRequest);
	r->instances = std::move(instances);
	r->volume_id = volume_id;
	r->position = block_pos;
	r->lod = lod;
	r->type = BlockDataRequest::TYPE_SAVE;
	r->block_size = volume.data_block_size;
	r->stream_dependency = volume.stream_dependency;
	r->request_instances = true;
	r->request_voxels = false;

	// No priority data, saving doesnt need sorting

	_streaming_thread_pool.enqueue(r);
}

void VoxelServer::request_block_generate_from_data_request(BlockDataRequest &src) {
	// This can be called from another thread

	BlockGenerateRequest *r = memnew(BlockGenerateRequest);
	r->voxels = src.voxels;
	r->volume_id = src.volume_id;
	r->position = src.position;
	r->lod = src.lod;
	r->block_size = src.block_size;
	r->stream_dependency = src.stream_dependency;
	r->priority_dependency = src.priority_dependency;

	_general_thread_pool.enqueue(r);
}

void VoxelServer::request_block_save_from_generate_request(BlockGenerateRequest &src) {
	// This can be called from another thread

	PRINT_VERBOSE(String("Requesting save of generator output for block {0} lod {1}")
						  .format(varray(src.position.to_vec3(), src.lod)));

	BlockDataRequest *r = memnew(BlockDataRequest());
	// TODO Optimization: `r->voxels` doesnt actually need to be shared
	r->voxels = gd_make_shared<VoxelBufferInternal>();
	src.voxels->duplicate_to(*r->voxels, true);
	r->volume_id = src.volume_id;
	r->position = src.position;
	r->lod = src.lod;
	r->type = BlockDataRequest::TYPE_SAVE;
	r->block_size = src.block_size;
	r->stream_dependency = src.stream_dependency;

	// No instances, generators are not designed to produce them at this stage yet.
	// No priority data, saving doesnt need sorting

	_streaming_thread_pool.enqueue(r);
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

bool VoxelServer::viewer_exists(uint32_t viewer_id) const {
	return _world.viewers.is_valid(viewer_id);
}

void VoxelServer::push_time_spread_task(IVoxelTimeSpreadTask *task) {
	_time_spread_task_runner.push(task);
}

int VoxelServer::get_main_thread_time_budget_usec() const {
	return _main_thread_time_budget_usec;
}

void VoxelServer::process() {
	// Note, this shouldn't be here. It should normally done just after SwapBuffers.
	// Godot does not have any C++ profiler usage anywhere, so when using Tracy Profiler I have to put it somewhere...
	// TODO Could connect to VisualServer end_frame_draw signal? How to make sure the singleton is available?
	//VOXEL_PROFILE_MARK_FRAME();
	VOXEL_PROFILE_SCOPE();

	// Receive data updates
	_streaming_thread_pool.dequeue_completed_tasks([this](IVoxelTask *task) {
		task->apply_result();
		memdelete(task);
	});

	// Receive generation and meshing results
	_general_thread_pool.dequeue_completed_tasks([this](IVoxelTask *task) {
		task->apply_result();
		memdelete(task);
	});

	// Run this after dequeueing threaded tasks, because they can add some to this runner,
	// which could in turn complete right away (we avoid 1-frame delays this way).
	_time_spread_task_runner.process(_main_thread_time_budget_usec);

	// Update viewer dependencies
	{
		const size_t viewer_count = _world.viewers.count();
		if (_world.shared_priority_dependency->viewers.size() != viewer_count) {
			// TODO We can avoid the invalidation by using an atomic size or memory barrier?
			_world.shared_priority_dependency = gd_make_shared<PriorityDependencyShared>();
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

static unsigned int debug_get_active_thread_count(const VoxelThreadPool &pool) {
	unsigned int active_count = 0;
	for (unsigned int i = 0; i < pool.get_thread_count(); ++i) {
		VoxelThreadPool::State s = pool.get_thread_debug_state(i);
		if (s == VoxelThreadPool::STATE_RUNNING) {
			++active_count;
		}
	}
	return active_count;
}

static VoxelServer::Stats::ThreadPoolStats debug_get_pool_stats(const VoxelThreadPool &pool) {
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
	s.generation_tasks = g_debug_generate_tasks_count;
	s.meshing_tasks = g_debug_mesh_tasks_count;
	s.streaming_tasks = g_debug_stream_tasks_count;
	s.main_thread_tasks = _time_spread_task_runner.get_pending_count();
	return s;
}

Dictionary VoxelServer::_b_get_stats() {
	return get_stats().to_dict();
}

void VoxelServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_stats"), &VoxelServer::_b_get_stats);
}

//----------------------------------------------------------------------------------------------------------------------

VoxelServer::BlockDataRequest::BlockDataRequest() {
	++g_debug_stream_tasks_count;
}

VoxelServer::BlockDataRequest::~BlockDataRequest() {
	--g_debug_stream_tasks_count;
}

void VoxelServer::BlockDataRequest::run(VoxelTaskContext ctx) {
	VOXEL_PROFILE_SCOPE();

	CRASH_COND(stream_dependency == nullptr);
	Ref<VoxelStream> stream = stream_dependency->stream;
	CRASH_COND(stream.is_null());

	const Vector3i origin_in_voxels = (position << lod) * block_size;

	switch (type) {
		case TYPE_LOAD: {
			ERR_FAIL_COND(voxels != nullptr);
			voxels = gd_make_shared<VoxelBufferInternal>();
			voxels->create(block_size, block_size, block_size);

			// TODO We should consider batching this again, but it needs to be done carefully.
			// Each task is one block, and priority depends on distance to closest viewer.
			// If we batch blocks, we have to do it by distance too.

			// TODO Assign max_lod_hint when available

			const VoxelStream::Result voxel_result = stream->emerge_block(*voxels, origin_in_voxels, lod);

			if (voxel_result == VoxelStream::RESULT_ERROR) {
				ERR_PRINT("Error loading voxel block");

			} else if (voxel_result == VoxelStream::RESULT_BLOCK_NOT_FOUND) {
				Ref<VoxelGenerator> generator = stream_dependency->generator;
				if (generator.is_valid()) {
					VoxelServer::get_singleton()->request_block_generate_from_data_request(*this);
					type = TYPE_FALLBACK_ON_GENERATOR;
				} else {
					// If there is no generator... what do we do? What defines the format of that empty block?
					// If the user leaves the defaults it's fine, but otherwise blocks of inconsistent format can
					// end up in the volume and that can cause errors.
					// TODO Define format on volume?
				}
			}

			if (request_instances && stream->supports_instance_blocks()) {
				ERR_FAIL_COND(instances != nullptr);

				VoxelStreamInstanceDataRequest instance_data_request;
				instance_data_request.lod = lod;
				instance_data_request.position = position;
				VoxelStream::Result instances_result;
				stream->load_instance_blocks(
						Span<VoxelStreamInstanceDataRequest>(&instance_data_request, 1),
						Span<VoxelStream::Result>(&instances_result, 1));

				if (instances_result == VoxelStream::RESULT_ERROR) {
					ERR_PRINT("Error loading instance block");

				} else if (voxel_result == VoxelStream::RESULT_BLOCK_FOUND) {
					instances = std::move(instance_data_request.data);
				}
				// If not found, instances will return null,
				// which means it can be generated by the instancer after the meshing process
			}
		} break;

		case TYPE_SAVE: {
			if (request_voxels) {
				ERR_FAIL_COND(voxels == nullptr);
				VoxelBufferInternal voxels_copy;
				{
					RWLockRead lock(voxels->get_lock());
					// TODO Optimization: is that copy necessary? It's possible it was already done while issuing the request
					voxels->duplicate_to(voxels_copy, true);
				}
				voxels = nullptr;
				stream->immerge_block(voxels_copy, origin_in_voxels, lod);
			}

			if (request_instances && stream->supports_instance_blocks()) {
				// If the provided data is null, it means this instance block was never modified.
				// Since we are in a save request, the saved data will revert to unmodified.
				// On the other hand, if we want to represent the fact that "everything was deleted here",
				// this should not be null.

				PRINT_VERBOSE(String("Saving instance block {0} lod {1} with data {2}")
									  .format(varray(position.to_vec3(), lod, ptr2s(instances.get()))));

				VoxelStreamInstanceDataRequest instance_data_request;
				instance_data_request.lod = lod;
				instance_data_request.position = position;
				instance_data_request.data = std::move(instances);
				stream->save_instance_blocks(Span<VoxelStreamInstanceDataRequest>(&instance_data_request, 1));
			}
		} break;

		default:
			CRASH_NOW_MSG("Invalid type");
	}

	has_run = true;
}

int VoxelServer::BlockDataRequest::get_priority() {
	if (type == TYPE_SAVE) {
		return 0;
	}
	float closest_viewer_distance_sq;
	const int p = VoxelServer::get_priority(priority_dependency, lod, &closest_viewer_distance_sq);
	too_far = closest_viewer_distance_sq > priority_dependency.drop_distance_squared;
	return p;
}

bool VoxelServer::BlockDataRequest::is_cancelled() {
	return type == TYPE_LOAD && (!stream_dependency->valid || too_far);
}

void VoxelServer::BlockDataRequest::apply_result() {
	Volume *volume = VoxelServer::get_singleton()->_world.volumes.try_get(volume_id);

	if (volume != nullptr) {
		// TODO Comparing pointer may not be guaranteed
		// The request response must match the dependency it would have been requested with.
		// If it doesn't match, we are no longer interested in the result.
		if (stream_dependency == volume->stream_dependency && type != BlockDataRequest::TYPE_FALLBACK_ON_GENERATOR) {
			BlockDataOutput o;
			o.voxels = voxels;
			o.instances = std::move(instances);
			o.position = position;
			o.lod = lod;
			o.dropped = !has_run;
			o.max_lod_hint = max_lod_hint;

			switch (type) {
				case BlockDataRequest::TYPE_SAVE:
					o.type = BlockDataOutput::TYPE_SAVE;
					break;

				case BlockDataRequest::TYPE_LOAD:
					o.type = BlockDataOutput::TYPE_LOAD;
					break;

				default:
					CRASH_NOW_MSG("Unexpected data request response type");
			}

			volume->reception_buffers->data_output.push_back(std::move(o));
		}

	} else {
		// This can happen if the user removes the volume while requests are still about to return
		PRINT_VERBOSE("Stream data request response came back but volume wasn't found");
	}
}

//----------------------------------------------------------------------------------------------------------------------

VoxelServer::BlockGenerateRequest::BlockGenerateRequest() {
	++g_debug_generate_tasks_count;
}

VoxelServer::BlockGenerateRequest::~BlockGenerateRequest() {
	--g_debug_generate_tasks_count;
}

void VoxelServer::BlockGenerateRequest::run(VoxelTaskContext ctx) {
	VOXEL_PROFILE_SCOPE();

	CRASH_COND(stream_dependency == nullptr);
	Ref<VoxelGenerator> generator = stream_dependency->generator;
	ERR_FAIL_COND(generator.is_null());

	const Vector3i origin_in_voxels = (position << lod) * block_size;

	if (voxels == nullptr) {
		voxels = gd_make_shared<VoxelBufferInternal>();
		voxels->create(block_size, block_size, block_size);
	}

	VoxelBlockRequest r{ *voxels, origin_in_voxels, lod };
	const VoxelGenerator::Result result = generator->generate_block(r);
	max_lod_hint = result.max_lod_hint;

	if (stream_dependency->valid) {
		Ref<VoxelStream> stream = stream_dependency->stream;
		if (stream.is_valid() && stream->get_save_generator_output()) {
			VoxelServer::get_singleton()->request_block_save_from_generate_request(*this);
		}
	}

	has_run = true;
}

int VoxelServer::BlockGenerateRequest::get_priority() {
	float closest_viewer_distance_sq;
	const int p = VoxelServer::get_priority(priority_dependency, lod, &closest_viewer_distance_sq);
	too_far = closest_viewer_distance_sq > priority_dependency.drop_distance_squared;
	return p;
}

bool VoxelServer::BlockGenerateRequest::is_cancelled() {
	return !stream_dependency->valid || too_far; // || stream_dependency->stream->get_fallback_generator().is_null();
}

void VoxelServer::BlockGenerateRequest::apply_result() {
	Volume *volume = VoxelServer::get_singleton()->_world.volumes.try_get(volume_id);

	if (volume != nullptr) {
		// TODO Comparing pointer may not be guaranteed
		// The request response must match the dependency it would have been requested with.
		// If it doesn't match, we are no longer interested in the result.
		if (stream_dependency == volume->stream_dependency) {
			BlockDataOutput o;
			o.voxels = voxels;
			o.position = position;
			o.lod = lod;
			o.dropped = !has_run;
			o.type = BlockDataOutput::TYPE_LOAD;
			o.max_lod_hint = max_lod_hint;
			volume->reception_buffers->data_output.push_back(std::move(o));
		}

	} else {
		// This can happen if the user removes the volume while requests are still about to return
		PRINT_VERBOSE("Gemerated data request response came back but volume wasn't found");
	}
}

//----------------------------------------------------------------------------------------------------------------------

// Takes a list of blocks and interprets it as a cube of blocks centered around the area we want to create a mesh from.
// Voxels from central blocks are copied, and part of side blocks are also copied so we get a temporary buffer
// which includes enough neighbors for the mesher to avoid doing bound checks.
static void copy_block_and_neighbors(Span<std::shared_ptr<VoxelBufferInternal>> blocks, VoxelBufferInternal &dst,
		int min_padding, int max_padding, int channels_mask) {
	VOXEL_PROFILE_SCOPE();

	// Extract wanted channels in a list
	FixedArray<uint8_t, VoxelBuffer::MAX_CHANNELS> channels;
	unsigned int channels_count = 0;
	for (unsigned int i = 0; i < VoxelBuffer::MAX_CHANNELS; ++i) {
		if ((channels_mask & (1 << i)) != 0) {
			channels[channels_count] = i;
			++channels_count;
		}
	}

	// Determine size of the cube of blocks
	int edge_size;
	int mesh_block_size_factor;
	switch (blocks.size()) {
		case 3 * 3 * 3:
			edge_size = 3;
			mesh_block_size_factor = 1;
			break;
		case 4 * 4 * 4:
			edge_size = 4;
			mesh_block_size_factor = 2;
			break;
		default:
			ERR_FAIL_MSG("Unsupported block count");
	}

	// Pick anchor block, usually within the central part of the cube (that block must be valid)
	const unsigned int anchor_buffer_index = edge_size * edge_size + edge_size + 1;

	std::shared_ptr<VoxelBufferInternal> &central_buffer = blocks[anchor_buffer_index];
	ERR_FAIL_COND_MSG(central_buffer == nullptr, "Central buffer must be valid");
	ERR_FAIL_COND_MSG(central_buffer->get_size().all_members_equal() == false, "Central buffer must be cubic");
	const int data_block_size = central_buffer->get_size().x;
	const int mesh_block_size = data_block_size * mesh_block_size_factor;
	const int padded_mesh_block_size = mesh_block_size + min_padding + max_padding;

	dst.create(padded_mesh_block_size, padded_mesh_block_size, padded_mesh_block_size);

	for (unsigned int ci = 0; ci < channels.size(); ++ci) {
		dst.set_channel_depth(ci, central_buffer->get_channel_depth(ci));
	}

	const Vector3i min_pos = -Vector3i(min_padding);
	const Vector3i max_pos = Vector3i(mesh_block_size + max_padding);

	// Using ZXY as convention to reconstruct positions with thread locking consistency
	unsigned int i = 0;
	for (int z = -1; z < edge_size - 1; ++z) {
		for (int x = -1; x < edge_size - 1; ++x) {
			for (int y = -1; y < edge_size - 1; ++y) {
				const Vector3i offset = data_block_size * Vector3i(x, y, z);
				const std::shared_ptr<VoxelBufferInternal> &src = blocks[i];
				++i;

				if (src == nullptr) {
					continue;
				}

				const Vector3i src_min = min_pos - offset;
				const Vector3i src_max = max_pos - offset;

				{
					RWLockRead read(src->get_lock());
					for (unsigned int ci = 0; ci < channels_count; ++ci) {
						dst.copy_from(*src, src_min, src_max, Vector3i(), channels[ci]);
					}
				}
			}
		}
	}
}

VoxelServer::BlockMeshRequest::BlockMeshRequest() {
	++g_debug_mesh_tasks_count;
}

VoxelServer::BlockMeshRequest::~BlockMeshRequest() {
	--g_debug_mesh_tasks_count;
}

void VoxelServer::BlockMeshRequest::run(VoxelTaskContext ctx) {
	VOXEL_PROFILE_SCOPE();
	CRASH_COND(meshing_dependency == nullptr);

	Ref<VoxelMesher> mesher = meshing_dependency->mesher;
	CRASH_COND(mesher.is_null());
	const unsigned int min_padding = mesher->get_minimum_padding();
	const unsigned int max_padding = mesher->get_maximum_padding();

	// TODO Cache?
	VoxelBufferInternal voxels;
	copy_block_and_neighbors(to_span(blocks, blocks_count),
			voxels, min_padding, max_padding, mesher->get_used_channels_mask());

	const VoxelMesher::Input input = { voxels, lod };
	mesher->build(surfaces_output, input);

	has_run = true;
}

int VoxelServer::BlockMeshRequest::get_priority() {
	float closest_viewer_distance_sq;
	const int p = VoxelServer::get_priority(priority_dependency, lod, &closest_viewer_distance_sq);
	too_far = closest_viewer_distance_sq > priority_dependency.drop_distance_squared;
	return p;
}

bool VoxelServer::BlockMeshRequest::is_cancelled() {
	return !meshing_dependency->valid || too_far;
}

void VoxelServer::BlockMeshRequest::apply_result() {
	Volume *volume = VoxelServer::get_singleton()->_world.volumes.try_get(volume_id);

	if (volume != nullptr) {
		// TODO Comparing pointer may not be guaranteed
		// The request response must match the dependency it would have been requested with.
		// If it doesn't match, we are no longer interested in the result.
		if (volume->meshing_dependency == meshing_dependency) {
			BlockMeshOutput o;
			// TODO Check for invalidation due to property changes

			if (has_run) {
				o.type = BlockMeshOutput::TYPE_MESHED;
			} else {
				o.type = BlockMeshOutput::TYPE_DROPPED;
			}

			o.position = position;
			o.lod = lod;
			o.surfaces = surfaces_output;

			ERR_FAIL_COND(volume->reception_buffers->mesh_output_callback == nullptr);
			ERR_FAIL_COND(volume->reception_buffers->callback_data == nullptr);
			volume->reception_buffers->mesh_output_callback(volume->reception_buffers->callback_data, o);
		}

	} else {
		// This can happen if the user removes the volume while requests are still about to return
		PRINT_VERBOSE("Mesh request response came back but volume wasn't found");
	}
}

//----------------------------------------------------------------------------------------------------------------------

namespace {
bool g_updater_created = false;
}

VoxelServerUpdater::VoxelServerUpdater() {
	PRINT_VERBOSE("Creating VoxelServerUpdater");
	set_process(true);
	g_updater_created = true;
}

VoxelServerUpdater::~VoxelServerUpdater() {
	g_updater_created = false;
}

void VoxelServerUpdater::ensure_existence(SceneTree *st) {
	if (st == nullptr) {
		return;
	}
	if (g_updater_created) {
		return;
	}
	Viewport *root = st->get_root();
	for (int i = 0; i < root->get_child_count(); ++i) {
		VoxelServerUpdater *u = Object::cast_to<VoxelServerUpdater>(root->get_child(i));
		if (u != nullptr) {
			return;
		}
	}
	VoxelServerUpdater *u = memnew(VoxelServerUpdater);
	u->set_name("VoxelServerUpdater_dont_touch_this");
	root->add_child(u);
}

void VoxelServerUpdater::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_PROCESS:
			// To workaround the absence of API to have a custom server processing in the main loop
			VoxelServer::get_singleton()->process();
			break;

		case NOTIFICATION_PREDELETE:
			PRINT_VERBOSE("Deleting VoxelServerUpdater");
			break;

		default:
			break;
	}
}
