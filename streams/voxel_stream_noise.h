#ifndef VOXEL_STREAM_NOISE_H
#define VOXEL_STREAM_NOISE_H

#include "../util/float_buffer_3d.h"
#include "voxel_stream.h"
#include <modules/opensimplex/open_simplex_noise.h>

class VoxelStreamNoise : public VoxelStream {
	GDCLASS(VoxelStreamNoise, VoxelStream)
public:
	void set_noise(Ref<OpenSimplexNoise> noise);
	Ref<OpenSimplexNoise> get_noise() const;

	void set_height_start(real_t y);
	real_t get_height_start() const;

	void set_height_range(real_t hrange);
	real_t get_height_range() const;

	void emerge_block(Ref<VoxelBuffer> out_buffer, Vector3i origin_in_voxels, int lod);

protected:
	static void _bind_methods();

private:
	Ref<OpenSimplexNoise> _noise;
	FloatBuffer3D _noise_buffer;
	float _height_start = 0;
	float _height_range = 300;
};

#endif // VOXEL_STREAM_NOISE_H
