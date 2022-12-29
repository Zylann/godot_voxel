#ifndef VOXEL_RENDER_DETAIL_TEXTURE_TASK_H
#define VOXEL_RENDER_DETAIL_TEXTURE_TASK_H

#include "../generators/voxel_generator.h"
#include "../meshers/voxel_mesher.h"
#include "../util/memory.h"
#include "../util/tasks/threaded_task.h"
#include "detail_rendering.h"
#include "ids.h"
#include "priority_dependency.h"

namespace zylann::voxel {

class RenderDetailTextureGPUTask;

// Renders textures providing extra details to far away voxel meshes.
// This is separate from the meshing task because it takes significantly longer to complete. It has different priority
// so most of the time we can get the mesh earlier and affine later with the results.
class RenderDetailTextureTask : public IThreadedTask {
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
	DetailRenderingSettings detail_texture_settings;

	// Output (to be assigned so it can be populated)
	std::shared_ptr<DetailTextureOutput> output_textures;

	// Identification
	Vector3i mesh_block_position;
	VolumeID volume_id;
	PriorityDependency priority_dependency;

	const char *get_debug_name() const override {
		return "RenderDetailTexture";
	}

	void run(ThreadedTaskContext ctx) override;
	void apply_result() override;
	TaskPriority get_priority() override;
	bool is_cancelled() override;

	// This is exposed for testing
	RenderDetailTextureGPUTask *make_gpu_task();

private:
	void run_on_cpu();
	void run_on_gpu();
};

// Performs final operations on the CPU after the GPU work is done
class RenderDetailTexturePass2Task : public IThreadedTask {
public:
	PackedByteArray atlas_data;
	DetailTextureData edited_tiles_texture_data;
	std::vector<DetailTextureData::Tile> tile_data;
	std::shared_ptr<DetailTextureOutput> output_textures;
	VolumeID volume_id;
	Vector3i mesh_block_position;
	Vector3i mesh_block_size;
	uint16_t atlas_width;
	uint16_t atlas_height;
	uint8_t lod_index;
	uint8_t tile_size_pixels;

	const char *get_debug_name() const override {
		return "RenderDetailTexturePass2";
	}

	void run(ThreadedTaskContext ctx) override;
	void apply_result() override;
};

} // namespace zylann::voxel

#endif // VOXEL_RENDER_DETAIL_TEXTURE_TASK_H
