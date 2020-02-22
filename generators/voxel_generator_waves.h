#ifndef VOXEL_GENERATOR_WAVES_H
#define VOXEL_GENERATOR_WAVES_H

#include "voxel_generator_heightmap.h"

class VoxelGeneratorWaves : public VoxelGeneratorHeightmap {
	GDCLASS(VoxelGeneratorWaves, VoxelGeneratorHeightmap)

public:
	VoxelGeneratorWaves();

	void set_channel(VoxelBuffer::ChannelId channel);
	VoxelBuffer::ChannelId get_channel() const;

	void generate_block(VoxelBlockRequest &input) override;

	Vector2 get_pattern_size() const { return _pattern_size; }
	void set_pattern_size(Vector2 size);

	Vector2 get_pattern_offset() const { return _pattern_offset; }
	void set_pattern_offset(Vector2 offset);

private:
	static void _bind_methods();

	Vector2 _pattern_size;
	Vector2 _pattern_offset;
};

#endif // VOXEL_GENERATOR_WAVES_H
