#ifndef VOXEL_GENERATOR_NOISE_H
#define VOXEL_GENERATOR_NOISE_H

#include "../../storage/voxel_buffer.h"
#include "../../storage/voxel_buffer_gd.h"
#include "../../util/godot/macros.h"
#include "../../util/thread/rw_lock.h"
#include "../voxel_generator.h"

ZN_GODOT_FORWARD_DECLARE(class Noise)

namespace zylann::voxel {

class VoxelGeneratorNoise : public VoxelGenerator {
	GDCLASS(VoxelGeneratorNoise, VoxelGenerator)

public:
	VoxelGeneratorNoise();
	~VoxelGeneratorNoise();

	void set_channel(VoxelBuffer::ChannelId p_channel);
	VoxelBuffer::ChannelId get_channel() const;

	int get_used_channels_mask() const override;

	void set_noise(Ref<Noise> noise);
	Ref<Noise> get_noise() const;

	void set_height_start(real_t y);
	real_t get_height_start() const;

	void set_height_range(real_t hrange);
	real_t get_height_range() const;

	Result generate_block(VoxelGenerator::VoxelQueryData &input) override;

private:
	void _on_noise_changed();

	void _b_set_channel(godot::VoxelBuffer::ChannelId p_channel);
	godot::VoxelBuffer::ChannelId _b_get_channel() const;

	static void _bind_methods();

	Ref<Noise> _noise;

	struct Parameters {
		VoxelBuffer::ChannelId channel = VoxelBuffer::CHANNEL_SDF;
		Ref<Noise> noise;
		float height_start = 0;
		float height_range = 300;
	};

	Parameters _parameters;
	RWLock _parameters_lock;
};

} // namespace zylann::voxel

#endif // VOXEL_GENERATOR_NOISE_H
