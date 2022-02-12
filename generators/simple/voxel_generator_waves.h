#ifndef VOXEL_GENERATOR_WAVES_H
#define VOXEL_GENERATOR_WAVES_H

#include "voxel_generator_heightmap.h"

namespace zylann::voxel {

class VoxelGeneratorWaves : public VoxelGeneratorHeightmap {
	GDCLASS(VoxelGeneratorWaves, VoxelGeneratorHeightmap)

public:
	VoxelGeneratorWaves();
	~VoxelGeneratorWaves();

	Result generate_block(VoxelGenerator::VoxelQueryData &input) override;

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

} // namespace zylann::voxel

#endif // VOXEL_GENERATOR_WAVES_H
