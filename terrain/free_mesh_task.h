#ifndef VOXEL_FREE_MESH_TASK_H
#define VOXEL_FREE_MESH_TASK_H

#include "../engine/voxel_engine.h"
#include "../util/godot/direct_mesh_instance.h"
#include "../util/profiling.h"
#include "../util/tasks/progressive_task_runner.h"

namespace zylann::voxel {

// Had to resort to this in Godot4 because deleting meshes is particularly expensive,
// because of the Vulkan allocator used by the renderer.
// It is a deferred cost (it is not spent at the exact time the Mesh object is destroyed, it happens later), so had to
// use a different type of task to load-balance it. What this task actually does is just to hold a reference on a mesh a
// bit longer, assuming that mesh is no longer used. Then the execution of the task releases that reference.
class FreeMeshTask : public IProgressiveTask {
public:
	static inline void try_add_and_destroy(zylann::godot::DirectMeshInstance &mi) {
		const Mesh *mesh = mi.get_mesh_ptr();
		if (mesh != nullptr && mesh->get_reference_count() == 1) {
			// That instances holds the last reference to this mesh
			add(mi.get_mesh());
		}
		mi.destroy();
	}

	void run() override {
		ZN_PROFILE_SCOPE();
		if (_mesh->get_reference_count() > 1) {
			ZN_PRINT_WARNING("Mesh has more than one ref left, task spreading will not be effective at smoothing "
							 "destruction cost");
		}
		_mesh.unref();
	}

private:
	static void add(Ref<Mesh> mesh) {
		ZN_ASSERT(mesh.is_valid());
		FreeMeshTask *task = ZN_NEW(FreeMeshTask(mesh));
		VoxelEngine::get_singleton().push_main_thread_progressive_task(task);
	}

	FreeMeshTask(Ref<Mesh> p_mesh) : _mesh(p_mesh) {}

	Ref<Mesh> _mesh;
};

} // namespace zylann::voxel

#endif // VOXEL_FREE_MESH_TASK_H
