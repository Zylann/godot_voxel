#ifndef VOXEL_MESH_BLOCK_TASK_H
#define VOXEL_MESH_BLOCK_TASK_H

#include "../constants/voxel_constants.h"
#include "../storage/voxel_buffer_internal.h"
#include "../util/godot/classes/array_mesh.h"
#include "../util/tasks/threaded_task.h"
#include "detail_rendering.h"
#include "ids.h"
#include "meshing_dependency.h"
#include "priority_dependency.h"

namespace zylann::voxel {

class VoxelData;

// Asynchronous task generating a mesh from voxel blocks and their neighbors, in a particular volume
class MeshBlockTask : public IThreadedTask {
public:
	MeshBlockTask();
	~MeshBlockTask();

	const char *get_debug_name() const override {
		return "MeshBlock";
	}

	void run(ThreadedTaskContext ctx) override;
	TaskPriority get_priority() override;
	bool is_cancelled() override;
	void apply_result() override;

	static int debug_get_running_count();

	// 3x3x3 or 4x4x4 grid of voxel blocks.
	FixedArray<std::shared_ptr<VoxelBufferInternal>, constants::MAX_BLOCK_COUNT_PER_REQUEST> blocks;
	// TODO Need to provide format
	// FixedArray<uint8_t, VoxelBufferInternal::MAX_CHANNELS> channel_depths;
	Vector3i mesh_block_position; // In mesh blocks of the specified lod
	VolumeID volume_id;
	uint8_t lod_index = 0;
	uint8_t blocks_count = 0;
	uint8_t data_block_size = 0;
	bool collision_hint = false;
	bool lod_hint = false;
	// Virtual textures might be enabled, but we don't always want to update them in every mesh update.
	// So this boolean is also checked to know if they should be computed.
	bool require_virtual_texture = false;
	uint8_t virtual_texture_generator_override_begin_lod_index = 0;
	bool virtual_texture_use_gpu = false;
	PriorityDependency priority_dependency;
	std::shared_ptr<MeshingDependency> meshing_dependency;
	std::shared_ptr<VoxelData> data;
	DetailRenderingSettings detail_texture_settings;
	Ref<VoxelGenerator> detail_texture_generator_override;

private:
	bool _has_run = false;
	bool _too_far = false;
	bool _has_mesh_resource = false;
	VoxelMesher::Output _surfaces_output;
	Ref<Mesh> _mesh;
	std::vector<uint8_t> _mesh_material_indices; // Indexed by mesh surface
	std::shared_ptr<DetailTextureOutput> _detail_textures;
};

Ref<ArrayMesh> build_mesh(Span<const VoxelMesher::Output::Surface> surfaces, Mesh::PrimitiveType primitive, int flags,
		std::vector<uint8_t> &surface_indices);

} // namespace zylann::voxel

#endif // VOXEL_MESH_BLOCK_TASK_H
