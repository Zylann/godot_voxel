#ifndef VOXEL_GENERATOR_NOISE_2D_H
#define VOXEL_GENERATOR_NOISE_2D_H

#include "../../util/macros.h"
#include "voxel_generator_heightmap.h"

ZN_GODOT_FORWARD_DECLARE(class Curve)
ZN_GODOT_FORWARD_DECLARE(class Noise)

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

	bool supports_series_generation() const override {
		return true;
	}

	void generate_series(Span<const float> positions_x, Span<const float> positions_y, Span<const float> positions_z,
			unsigned int channel, Span<float> out_values, Vector3f min_pos, Vector3f max_pos) override;

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
