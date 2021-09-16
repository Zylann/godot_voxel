#ifndef VOXEL_GENERATOR_NOISE_2D_H
#define VOXEL_GENERATOR_NOISE_2D_H

#include "voxel_generator_heightmap.h"
#include <modules/opensimplex/open_simplex_noise.h>

class VoxelGeneratorNoise2D : public VoxelGeneratorHeightmap {
	GDCLASS(VoxelGeneratorNoise2D, VoxelGeneratorHeightmap)

public:
	VoxelGeneratorNoise2D();
	~VoxelGeneratorNoise2D();

	void set_noise(Ref<OpenSimplexNoise> noise);
	Ref<OpenSimplexNoise> get_noise() const;

	void set_curve(Ref<Curve> curve);
	Ref<Curve> get_curve() const;

	Result generate_block(VoxelBlockRequest &input) override;

private:
	void _on_noise_changed();
	void _on_curve_changed();

	static void _bind_methods();

private:
	Ref<OpenSimplexNoise> _noise;
	Ref<Curve> _curve;

	struct Parameters {
		Ref<OpenSimplexNoise> noise;
		Ref<Curve> curve;
	};

	Parameters _parameters;
	RWLock _parameters_lock;
};

#endif // VOXEL_GENERATOR_NOISE_2D_H
