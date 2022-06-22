#include "build_transition_mesh_task.h"
#include "voxel_lod_terrain.h"

namespace zylann::voxel {

void BuildTransitionMeshTask::run(TimeSpreadTaskContext &ctx) {
	ZN_PROFILE_SCOPE();
	if (block == nullptr) {
		// Got cancelled
		return;
	}
	if (!VoxelServer::get_singleton().is_volume_valid(volume_id)) {
		// Terrain got destroyed
		return;
	}

	Ref<Material> material = volume->get_material();
	const Transform3D transform = volume->get_global_transform();
	const DirectMeshInstance::GIMode gi_mode = DirectMeshInstance::GIMode(volume->get_gi_mode());

	Ref<ArrayMesh> transition_mesh =
			build_mesh(to_span(mesh_data), Mesh::PrimitiveType(primitive_type), mesh_flags, material);

	block->set_transition_mesh(transition_mesh, side, gi_mode);

	if (transition_mesh.is_valid()) {
		block->update_transition_mesh_transform(side, transform);
	}

	block->on_transition_mesh_task_completed(*this);
}

} // namespace zylann::voxel
