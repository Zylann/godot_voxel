#include "voxel_engine_gd.h"
#include "../storage/voxel_memory_pool.h"
#include "../util/macros.h"
#include "../util/profiling.h"
#include "../util/tasks/godot/threaded_task_gd.h"
#include "voxel_engine.h"

namespace zylann::voxel::gd {
VoxelEngine *g_voxel_engine = nullptr;

VoxelEngine *VoxelEngine::get_singleton() {
	CRASH_COND_MSG(g_voxel_engine == nullptr, "Accessing singleton while it's null");
	return g_voxel_engine;
}

void VoxelEngine::create_singleton() {
	CRASH_COND_MSG(g_voxel_engine != nullptr, "Creating singleton twice");
	g_voxel_engine = memnew(VoxelEngine);
}

void VoxelEngine::destroy_singleton() {
	CRASH_COND_MSG(g_voxel_engine == nullptr, "Destroying singleton twice");
	memdelete(g_voxel_engine);
	g_voxel_engine = nullptr;
}

VoxelEngine::VoxelEngine() {
#ifdef ZN_PROFILER_ENABLED
	CRASH_COND(RenderingServer::get_singleton() == nullptr);
	RenderingServer::get_singleton()->connect(
			SNAME("frame_post_draw"), callable_mp(this, &VoxelEngine::_on_rendering_server_frame_post_draw));
#endif
}

Dictionary to_dict(const zylann::voxel::VoxelEngine::Stats::ThreadPoolStats &stats) {
	Dictionary d;
	d["tasks"] = stats.tasks;
	d["active_threads"] = stats.active_threads;
	d["thread_count"] = stats.thread_count;
	return d;
}

Dictionary to_dict(const zylann::voxel::VoxelEngine::Stats &stats) {
	Dictionary pools;
	pools["general"] = to_dict(stats.general);

	Dictionary tasks;
	tasks["streaming"] = stats.streaming_tasks;
	tasks["generation"] = stats.generation_tasks;
	tasks["meshing"] = stats.meshing_tasks;
	tasks["main_thread"] = stats.main_thread_tasks;

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

Dictionary VoxelEngine::get_stats() const {
	return to_dict(zylann::voxel::VoxelEngine::get_singleton().get_stats());
}

void VoxelEngine::schedule_task(Ref<ZN_ThreadedTask> task) {
	ERR_FAIL_COND(task.is_null());
	ERR_FAIL_COND_MSG(task->is_scheduled(), "Cannot schedule again a task that is already scheduled");
	zylann::voxel::VoxelEngine::get_singleton().push_async_task(task->create_task());
}

void VoxelEngine::_on_rendering_server_frame_post_draw() {
#ifdef ZN_PROFILER_ENABLED
	ZN_PROFILE_MARK_FRAME();
#endif
}

#ifdef TOOLS_ENABLED

void VoxelEngine::set_editor_camera_info(Vector3 position, Vector3 direction) {
	_editor_camera_position = position;
	_editor_camera_direction = direction;
}

Vector3 VoxelEngine::get_editor_camera_position() const {
	return _editor_camera_position;
}

Vector3 VoxelEngine::get_editor_camera_direction() const {
	return _editor_camera_direction;
}

#endif

void VoxelEngine::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_stats"), &VoxelEngine::get_stats);
}

} // namespace zylann::voxel::gd
