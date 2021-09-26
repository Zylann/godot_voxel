#ifndef VOXEL_GENERATOR_FLAT_H
#define VOXEL_GENERATOR_FLAT_H

#include "../voxel_generator.h"

class VoxelGeneratorFlat : public VoxelGenerator {
	GDCLASS(VoxelGeneratorFlat, VoxelGenerator)

public:
	VoxelGeneratorFlat();
	~VoxelGeneratorFlat();

	void set_channel(VoxelBuffer::ChannelId p_channel);
	VoxelBuffer::ChannelId get_channel() const;
	int get_used_channels_mask() const override;

	Result generate_block(VoxelBlockRequest &input) override;

	void set_voxel_type(int t);
	int get_voxel_type() const;

	void set_height(float h);
	float get_height() const;

protected:
	static void _bind_methods();

private:
	struct Parameters {
		VoxelBufferInternal::ChannelId channel = VoxelBufferInternal::CHANNEL_SDF;
		int voxel_type = 1;
		float height = 0;
		float iso_scale = 0.1;
	};

	Parameters _parameters;
	RWLock _parameters_lock;
};

#endif // VOXEL_GENERATOR_FLAT_H
