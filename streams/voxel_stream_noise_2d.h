#ifndef VOXEL_STREAM_NOISE_2D_H
#define VOXEL_STREAM_NOISE_2D_H

#include "voxel_stream_heightmap.h"
#include <modules/opensimplex/open_simplex_noise.h>

class VoxelStreamNoise2D : public VoxelStreamHeightmap {
	GDCLASS(VoxelStreamNoise2D, VoxelStreamHeightmap)
public:
	VoxelStreamNoise2D();

	void set_noise(Ref<OpenSimplexNoise> noise);
	Ref<OpenSimplexNoise> get_noise() const;

	void emerge_block(Ref<VoxelBuffer> p_out_buffer, Vector3i origin_in_voxels, int lod);

private:
	static void _bind_methods();

private:
	Ref<OpenSimplexNoise> _noise;
};

#endif // VOXEL_STREAM_NOISE_2D_H
