#ifndef VOXEL_GENERATOR_SCRIPT_H
#define VOXEL_GENERATOR_SCRIPT_H

#include "voxel_generator.h"

// Generator based on a script, like GDScript, C# or NativeScript.
// The script is expected to properly handle multithreading.
class VoxelGeneratorScript : public VoxelGenerator {
	GDCLASS(VoxelGeneratorScript, VoxelGenerator)
public:
	VoxelGeneratorScript();

	Result generate_block(VoxelBlockRequest &input) override;
	int get_used_channels_mask() const override;

private:
	static void _bind_methods();
};

#endif // VOXEL_GENERATOR_SCRIPT_H
