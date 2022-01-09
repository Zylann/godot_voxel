#include "voxel_server_gd.h"
#include "../util/macros.h"
#include "../util/profiling.h"
#include "voxel_server.h"

namespace zylann::voxel::gd {
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
#ifdef VOXEL_PROFILER_ENABLED
	CRASH_COND(RenderingServer::get_singleton() == nullptr);
	RenderingServer::get_singleton()->connect(
			SNAME("frame_post_draw"), callable_mp(this, &VoxelServer::_on_rendering_server_frame_post_draw));
#endif
}

Dictionary VoxelServer::get_stats() const {
	return zylann::voxel::VoxelServer::get_singleton()->get_stats().to_dict();
}

void VoxelServer::_on_rendering_server_frame_post_draw() {
#ifdef VOXEL_PROFILER_ENABLED
	VOXEL_PROFILE_MARK_FRAME();
#endif
}

void VoxelServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_stats"), &VoxelServer::get_stats);
}

} // namespace zylann::voxel::gd
