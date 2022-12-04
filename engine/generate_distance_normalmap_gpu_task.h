#ifndef VOXEL_GENERATE_DISTANCE_NORMALMAP_GPU_TASK_H
#define VOXEL_GENERATE_DISTANCE_NORMALMAP_GPU_TASK_H

#include "../util/math/vector3f.h"
#include "../util/math/vector4f.h"
#include "distance_normalmaps.h"
#include "gpu_task_runner.h"

namespace zylann::voxel {

class ComputeShader;

class GenerateDistanceNormalMapGPUTask : public IGPUTask {
public:
	uint16_t texture_width;
	uint16_t texture_height;

	// Using 4-component vectors to match alignment rules.
	// See https://stackoverflow.com/a/38172697
	std::vector<Vector4f> mesh_vertices;
	std::vector<int32_t> mesh_indices;
	std::vector<int32_t> cell_triangles;

	struct TileData {
		uint8_t cell_x = 0;
		uint8_t cell_y = 0;
		uint8_t cell_z = 0;
		uint8_t _pad = 0;
		// aaaaaaaa aaaaaaaa aaaaaaaa 0bbb00cc
		// a: 24-bit index into `u_cell_tris.data` array.
		// b: 3-bit number of triangles.
		// c: 2-bit projection direction (0:X, 1:Y, 2:Z)
		uint32_t data = 0;
	};

	// Should fit as an `ivec2[]`
	std::vector<TileData> tile_data;

	struct Params {
		Vector3f block_origin_world;
		float pixel_world_step;
		float max_deviation_cosine;
		float max_deviation_sine;
		int32_t tile_size_pixels;
		int32_t tiles_x;
		int32_t tiles_y;
	};

	Params params;

	std::shared_ptr<ComputeShader> shader;

	// Stuff to carry over for the second CPU pass
	std::shared_ptr<VirtualTextureOutput> output;
	NormalMapData edited_tiles_normalmap_data;
	Vector3i block_position;
	Vector3i block_size;
	uint32_t volume_id;
	uint8_t lod_index;

	void prepare(GPUTaskContext &ctx) override;
	void collect(GPUTaskContext &ctx) override;

	// Exposed for testing
	PackedByteArray collect_texture_and_cleanup(RenderingDevice &rd);

private:
	RID _normalmap_texture0_rid;
	RID _normalmap_texture1_rid;
	RID _normalmap_rendering_pipeline_rid;
	RID _normalmap_dilation_pipeline_rid;
	RID _mesh_vertices_rid;
	RID _mesh_indices_rid;
	RID _cell_triangles_rid;
	RID _tile_data_rid;
	RID _normalmap_rendering_params_rid;
	RID _dilation_params_rid;
};

} // namespace zylann::voxel

#endif // VOXEL_GENERATE_DISTANCE_NORMALMAP_GPU_TASK_H
