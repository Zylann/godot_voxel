#include "voxel_engine_gd.h"
#include "../constants/version.gen.h"
#include "../constants/voxel_string_names.h"
#include "../storage/voxel_memory_pool.h"
#include "../util/godot/classes/project_settings.h"
#include "../util/godot/classes/rendering_server.h"
#include "../util/godot/core/packed_arrays.h"
#include "../util/macros.h"
#include "../util/profiling.h"
#include "../util/tasks/godot/threaded_task_gd.h"
#include "voxel_engine.h"

using namespace zylann::godot;

namespace zylann::voxel::godot {
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

zylann::voxel::VoxelEngine::ThreadsConfig VoxelEngine::get_config_from_godot(
		unsigned int &out_main_thread_time_budget_usec) {
	ZN_ASSERT(ProjectSettings::get_singleton() != nullptr);
	ProjectSettings &ps = *ProjectSettings::get_singleton();

	zylann::voxel::VoxelEngine::ThreadsConfig config;

	// Compute thread count for general pool.

	add_custom_project_setting(Variant::INT, "voxel/threads/count/minimum", PROPERTY_HINT_RANGE, "1,64", 1, true);
	add_custom_project_setting(
			Variant::INT, "voxel/threads/count/margin_below_max", PROPERTY_HINT_RANGE, "1,64", 1, true);
	add_custom_project_setting(
			Variant::FLOAT, "voxel/threads/count/ratio_over_max", PROPERTY_HINT_RANGE, "0,1,0.1", 0.5f, true);
	add_custom_project_setting(
			Variant::INT, "voxel/threads/main/time_budget_ms", PROPERTY_HINT_RANGE, "0,1000", 8, true);

	out_main_thread_time_budget_usec = 1000 * int(ps.get("voxel/threads/main/time_budget_ms"));

	config.thread_count_minimum = math::max(1, int(ps.get("voxel/threads/count/minimum")));

	// How many threads below available count on the CPU should we set as limit
	config.thread_count_margin_below_max = math::max(1, int(ps.get("voxel/threads/count/margin_below_max")));

	// Portion of available CPU threads to attempt using
	config.thread_count_ratio_over_max = math::clamp(float(ps.get("voxel/threads/count/ratio_over_max")), 0.f, 1.f);

	return config;
}

VoxelEngine::VoxelEngine() {
#ifdef ZN_PROFILER_ENABLED
	CRASH_COND(RenderingServer::get_singleton() == nullptr);
	RenderingServer::get_singleton()->connect(VoxelStringNames::get_singleton().frame_post_draw,
			callable_mp(this, &VoxelEngine::_on_rendering_server_frame_post_draw));
#endif
}

int VoxelEngine::get_version_major() const {
	return VOXEL_VERSION_MAJOR;
}

int VoxelEngine::get_version_minor() const {
	return VOXEL_VERSION_MINOR;
}

int VoxelEngine::get_version_patch() const {
	return VOXEL_VERSION_PATCH;
}

Dictionary to_dict(const zylann::voxel::VoxelEngine::Stats::ThreadPoolStats &stats) {
	Dictionary d;
	d["tasks"] = stats.tasks;
	d["active_threads"] = stats.active_threads;
	d["thread_count"] = stats.thread_count;

	PackedStringArray task_names;
	{
		task_names.resize(stats.thread_count);
		Span<String> task_names_s = to_span(task_names);
		for (unsigned int i = 0; i < stats.active_task_names.size(); ++i) {
			const char *name = stats.active_task_names[i];
			if (name != nullptr) {
				task_names_s[i] = name;
			}
		}
	}

	d["task_names"] = task_names;

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
#ifdef DEBUG_ENABLED
	const uint64_t std_allocated = static_cast<int64_t>(StdDefaultAllocatorCounters::g_allocated);
	const uint64_t std_deallocated = static_cast<int64_t>(StdDefaultAllocatorCounters::g_deallocated);
	mem["std_allocated"] = std_allocated;
	mem["std_deallocated"] = std_deallocated;
	mem["std_current"] = std_allocated - std_deallocated;
#else
	mem["std_allocated"] = -1;
	mem["std_deallocated"] = -1;
	mem["std_current"] = -1;
#endif

	Dictionary d;
	d["thread_pools"] = pools;
	d["tasks"] = tasks;
	d["memory_pools"] = mem;
	return d;
}

Dictionary VoxelEngine::get_stats() const {
	ZN_PROFILE_SCOPE();
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
	ClassDB::bind_method(D_METHOD("get_version_major"), &VoxelEngine::get_version_major);
	ClassDB::bind_method(D_METHOD("get_version_minor"), &VoxelEngine::get_version_minor);
	ClassDB::bind_method(D_METHOD("get_version_patch"), &VoxelEngine::get_version_patch);
	ClassDB::bind_method(D_METHOD("get_stats"), &VoxelEngine::get_stats);
}

} // namespace zylann::voxel::godot
