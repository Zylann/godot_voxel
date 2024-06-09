#include "generate_instances_block_task.h"
#include "../../util/godot/classes/array_mesh.h"
#include "../../util/profiling.h"

namespace zylann::voxel {

void GenerateInstancesBlockTask::run(ThreadedTaskContext &ctx) {
	ZN_PROFILE_SCOPE();
	ZN_ASSERT_RETURN(generator.is_valid());
	ZN_ASSERT(output_queue != nullptr);

	PackedVector3Array vertices = surface_arrays[ArrayMesh::ARRAY_VERTEX];
	ZN_ASSERT_RETURN(vertices.size() > 0);

	PackedVector3Array normals = surface_arrays[ArrayMesh::ARRAY_NORMAL];
	ZN_ASSERT_RETURN(normals.size() > 0);

	static thread_local StdVector<Transform3f> tls_generated_transforms;
	tls_generated_transforms.clear();

	const uint8_t gen_octant_mask = ~edited_mask;

	generator->generate_transforms(
			tls_generated_transforms,
			mesh_block_grid_position,
			lod_index,
			layer_id,
			surface_arrays,
			up_mode,
			gen_octant_mask,
			mesh_block_size
	);

	for (const Transform3f &t : tls_generated_transforms) {
		transforms.push_back(t);
	}

	{
		MutexLock mlock(output_queue->mutex);
		output_queue->results.push_back(InstanceLoadingTaskOutput());
		InstanceLoadingTaskOutput &o = output_queue->results.back();
		o.layer_id = layer_id;
		o.edited_mask = edited_mask;
		o.render_block_position = mesh_block_grid_position;
		o.transforms = std::move(transforms);
	}
}

} // namespace zylann::voxel
