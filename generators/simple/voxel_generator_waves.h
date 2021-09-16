#ifndef VOXEL_GENERATOR_WAVES_H
#define VOXEL_GENERATOR_WAVES_H

#include "voxel_generator_heightmap.h"

class VoxelGeneratorWaves : public VoxelGeneratorHeightmap {
	GDCLASS(VoxelGeneratorWaves, VoxelGeneratorHeightmap)

public:
	VoxelGeneratorWaves();
	~VoxelGeneratorWaves();

	void set_channel(VoxelBuffer::ChannelId channel);
	VoxelBuffer::ChannelId get_channel() const;

	Result generate_block(VoxelBlockRequest &input) override;

	Vector2 get_pattern_size() const;
	void set_pattern_size(Vector2 size);

	Vector2 get_pattern_offset() const;
	void set_pattern_offset(Vector2 offset);

private:
	static void _bind_methods();

	struct Parameters {
		Vector2 pattern_size;
		Vector2 pattern_offset;
	};

	Parameters _parameters;
	RWLock _parameters_lock;
};

#endif // VOXEL_GENERATOR_WAVES_H
