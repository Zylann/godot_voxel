#ifndef VOXEL_GENERATE_DISTANCE_NORMALMAP_TASK_H
#define VOXEL_GENERATE_DISTANCE_NORMALMAP_TASK_H

#include "../generators/voxel_generator.h"
#include "../meshers/voxel_mesher.h"
#include "../util/memory.h"
#include "../util/tasks/threaded_task.h"
#include "distance_normalmaps.h"
#include "ids.h"
#include "priority_dependency.h"

namespace zylann::voxel {

class GenerateDistanceNormalMapGPUTask;

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
	bool use_gpu = false;
	NormalMapSettings virtual_texture_settings;

	// Output (to be assigned so it can be populated)
	std::shared_ptr<VirtualTextureOutput> virtual_textures;

	// Identification
	Vector3i mesh_block_position;
	VolumeID volume_id;
	PriorityDependency priority_dependency;

	void run(ThreadedTaskContext ctx) override;
	void apply_result() override;
	TaskPriority get_priority() override;
	bool is_cancelled() override;

	// This is exposed for testing
	GenerateDistanceNormalMapGPUTask *make_gpu_task();

private:
	void run_on_cpu();
	void run_on_gpu();
};

// Performs final operations on the CPU after the GPU work is done
class RenderVirtualTexturePass2Task : public IThreadedTask {
public:
	PackedByteArray atlas_data;
	NormalMapData edited_tiles_normalmap_data;
	std::vector<NormalMapData::Tile> tile_data;
	std::shared_ptr<VirtualTextureOutput> virtual_textures;
	VolumeID volume_id;
	Vector3i mesh_block_position;
	Vector3i mesh_block_size;
	uint16_t atlas_width;
	uint16_t atlas_height;
	uint8_t lod_index;
	uint8_t tile_size_pixels;

	void run(ThreadedTaskContext ctx) override;
	void apply_result() override;
};

} // namespace zylann::voxel

#endif // VOXEL_GENERATE_DISTANCE_NORMALMAP_TASK_H
