#ifndef VOXEL_FREE_MESH_TASK_H
#define VOXEL_FREE_MESH_TASK_H

#include "../server/voxel_server.h"
#include "../util/godot/direct_mesh_instance.h"
#include "../util/tasks/progressive_task_runner.h"

namespace zylann::voxel {

// Had to resort to this in Godot4 because deleting meshes is particularly expensive,
// because of the Vulkan allocator used by the renderer.
// It is a deferred cost, so had to use a different type of task
class FreeMeshTask : public zylann::IProgressiveTask {
public:
	static inline void try_add_and_destroy(DirectMeshInstance &mi) {
		if (mi.get_mesh().is_valid()) {
			add(mi.get_mesh());
		}
		mi.destroy();
	}

	static void add(Ref<Mesh> mesh) {
		CRASH_COND(mesh.is_null());
		FreeMeshTask *task = memnew(FreeMeshTask(mesh));
		VoxelServer::get_singleton().push_main_thread_progressive_task(task);
	}

	FreeMeshTask(Ref<Mesh> p_mesh) : mesh(p_mesh) {}

	void run() override {
		ZN_PROFILE_SCOPE();
#ifdef DEBUG_ENABLED
		if (mesh->reference_get_count() > 1) {
			WARN_PRINT("Mesh has more than one ref left, task spreading will not be effective at smoothing "
					   "destruction cost");
		}
#endif
		mesh.unref();
	}

	Ref<Mesh> mesh;
};

} // namespace zylann::voxel

#endif // VOXEL_FREE_MESH_TASK_H
