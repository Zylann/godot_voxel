#ifndef VOXEL_GENERATE_DISTANCE_NORMALMAP_TASK_H
#define VOXEL_GENERATE_DISTANCE_NORMALMAP_TASK_H

#include "../generators/voxel_generator.h"
#include "../meshers/voxel_mesher.h"
#include "../util/memory.h"
#include "../util/tasks/threaded_task.h"
#include "distance_normalmaps.h"
#include "priority_dependency.h"

namespace zylann::voxel {

// Renders textures providing extra details to far away voxel meshes.
// This is separate from the meshing task because it takes significantly longer to complete. It has different priority
// so most of the time we can get the mesh earlier and affine later with the results.
class GenerateDistanceNormalmapTask : public IThreadedTask {
public:
	// Input

	UniquePtr<ICellIterator> cell_iterator;
	// TODO Optimize: perhaps we could find a way to not copy mesh data? The only reason is because Godot wants a
	// slightly different data structure potentially taking unnecessary doubles because it uses `Vector3`...
	std::vector<Vector3f> mesh_vertices;
	std::vector<Vector3f> mesh_normals;
	std::vector<int> mesh_indices;
	Ref<VoxelGenerator> generator;
	std::shared_ptr<VoxelData> voxel_data;
	Vector3i mesh_block_size;
	uint8_t lod_index;
	NormalMapSettings virtual_texture_settings;

	// Output (to be assigned so it can be populated)
	std::shared_ptr<VirtualTextureOutput> virtual_textures;

	// Identification
	Vector3i mesh_block_position;
	uint32_t volume_id;
	PriorityDependency priority_dependency;

	void run(ThreadedTaskContext ctx) override;
	void apply_result() override;
	TaskPriority get_priority() override;
	bool is_cancelled() override;
};

} // namespace zylann::voxel

#endif // VOXEL_GENERATE_DISTANCE_NORMALMAP_TASK_H
