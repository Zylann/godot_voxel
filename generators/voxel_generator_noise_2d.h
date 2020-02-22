#ifndef VOXEL_GENERATOR_NOISE_2D_H
#define VOXEL_GENERATOR_NOISE_2D_H

#include "voxel_generator_heightmap.h"
#include <modules/opensimplex/open_simplex_noise.h>

class VoxelGeneratorNoise2D : public VoxelGeneratorHeightmap {
	GDCLASS(VoxelGeneratorNoise2D, VoxelGeneratorHeightmap)
	
public:
	VoxelGeneratorNoise2D();

	void set_noise(Ref<OpenSimplexNoise> noise);
	Ref<OpenSimplexNoise> get_noise() const;

	void set_curve(Ref<Curve> curve);
	Ref<Curve> get_curve() const;

	void generate_block(VoxelBlockRequest &input) override;

private:
	static void _bind_methods();

private:
	Ref<OpenSimplexNoise> _noise;
	Ref<Curve> _curve;
};

#endif // VOXEL_GENERATOR_NOISE_2D_H
