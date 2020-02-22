#ifndef VOXEL_GENERATOR_FLAT_H
#define VOXEL_GENERATOR_FLAT_H

#include "voxel_generator.h"

class VoxelGeneratorFlat : public VoxelGenerator {
	GDCLASS(VoxelGeneratorFlat, VoxelGenerator)

public:
	VoxelGeneratorFlat();

	void set_channel(VoxelBuffer::ChannelId channel);
	VoxelBuffer::ChannelId get_channel() const;
	int get_used_channels_mask() const override;

	void generate_block(VoxelBlockRequest &input) override;

	void set_voxel_type(int t);
	int get_voxel_type() const;

	void set_height(float h);
	float get_height() const { return _height; }

protected:
	static void _bind_methods();

private:
	VoxelBuffer::ChannelId _channel = VoxelBuffer::CHANNEL_SDF;
	int _voxel_type = 1;
	float _height = 0;
	float _iso_scale = 0.1;
};

#endif // VOXEL_GENERATOR_FLAT_H
