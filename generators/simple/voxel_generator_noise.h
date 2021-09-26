#ifndef VOXEL_GENERATOR_NOISE_H
#define VOXEL_GENERATOR_NOISE_H

#include "../voxel_generator.h"
#include <modules/opensimplex/open_simplex_noise.h>

class VoxelGeneratorNoise : public VoxelGenerator {
	GDCLASS(VoxelGeneratorNoise, VoxelGenerator)

public:
	VoxelGeneratorNoise();
	~VoxelGeneratorNoise();

	void set_channel(VoxelBuffer::ChannelId p_channel);
	VoxelBuffer::ChannelId get_channel() const;
	int get_used_channels_mask() const override;

	void set_noise(Ref<OpenSimplexNoise> noise);
	Ref<OpenSimplexNoise> get_noise() const;

	void set_height_start(real_t y);
	real_t get_height_start() const;

	void set_height_range(real_t hrange);
	real_t get_height_range() const;

	Result generate_block(VoxelBlockRequest &input) override;

protected:
	void _on_noise_changed();

	static void _bind_methods();

private:
	Ref<OpenSimplexNoise> _noise;

	struct Parameters {
		VoxelBufferInternal::ChannelId channel = VoxelBufferInternal::CHANNEL_SDF;
		Ref<OpenSimplexNoise> noise;
		float height_start = 0;
		float height_range = 300;
	};

	Parameters _parameters;
	RWLock _parameters_lock;
};

#endif // VOXEL_GENERATOR_NOISE_H
