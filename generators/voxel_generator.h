#ifndef VOXEL_GENERATOR_H
#define VOXEL_GENERATOR_H

#include "../streams/voxel_block_request.h"
#include <core/resource.h>

// Provides access to read-only generated voxels.
// Must be implemented in a multi-thread-safe way.
class VoxelGenerator : public Resource {
	GDCLASS(VoxelGenerator, Resource)
public:
	VoxelGenerator();

	virtual void generate_block(VoxelBlockRequest &input);
	// TODO Single sample

	// Declares the channels this generator will use
	virtual int get_used_channels_mask() const;

protected:
	static void _bind_methods();

	void _b_generate_block(Ref<VoxelBuffer> out_buffer, Vector3 origin_in_voxels, int lod);
};

#endif // VOXEL_GENERATOR_H
