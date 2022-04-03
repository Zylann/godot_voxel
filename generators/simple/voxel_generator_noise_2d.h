#ifndef VOXEL_GENERATOR_NOISE_2D_H
#define VOXEL_GENERATOR_NOISE_2D_H

#include "voxel_generator_heightmap.h"

class Curve;
class Noise;

namespace zylann::voxel {

class VoxelGeneratorNoise2D : public VoxelGeneratorHeightmap {
	GDCLASS(VoxelGeneratorNoise2D, VoxelGeneratorHeightmap)

public:
	VoxelGeneratorNoise2D();
	~VoxelGeneratorNoise2D();

	void set_noise(Ref<Noise> noise);
	Ref<Noise> get_noise() const;

	void set_curve(Ref<Curve> curve);
	Ref<Curve> get_curve() const;

	Result generate_block(VoxelGenerator::VoxelQueryData &input) override;

private:
	void _on_noise_changed();
	void _on_curve_changed();

	static void _bind_methods();

private:
	Ref<Noise> _noise;
	Ref<Curve> _curve;

	struct Parameters {
		Ref<Noise> noise;
		Ref<Curve> curve;
	};

	Parameters _parameters;
	RWLock _parameters_lock;
};

} // namespace zylann::voxel

#endif // VOXEL_GENERATOR_NOISE_2D_H
