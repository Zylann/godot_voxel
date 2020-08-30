#ifndef VOXEL_GENERATOR_NOISE_H
#define VOXEL_GENERATOR_NOISE_H

#include "../util/float_buffer_3d.h"
#include "voxel_generator.h"
#include <modules/opensimplex/open_simplex_noise.h>

class VoxelGeneratorNoise : public VoxelGenerator {
	GDCLASS(VoxelGeneratorNoise, VoxelGenerator)

public:
	VoxelGeneratorNoise();

	void set_channel(VoxelBuffer::ChannelId channel);
	VoxelBuffer::ChannelId get_channel() const;
	int get_used_channels_mask() const override;

	void set_noise(Ref<OpenSimplexNoise> noise);
	Ref<OpenSimplexNoise> get_noise() const;

	void set_height_start(real_t y);
	real_t get_height_start() const;

	void set_height_range(real_t hrange);
	real_t get_height_range() const;

	void generate_block(VoxelBlockRequest &input) override;

protected:
	static void _bind_methods();

private:
	VoxelBuffer::ChannelId _channel = VoxelBuffer::CHANNEL_SDF;
	Ref<OpenSimplexNoise> _noise;
	FloatBuffer3D _noise_buffer;
	float _height_start = 0;
	float _height_range = 300;
};

#endif // VOXEL_GENERATOR_NOISE_H
