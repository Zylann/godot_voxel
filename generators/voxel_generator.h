#ifndef VOXEL_GENERATOR_H
#define VOXEL_GENERATOR_H

#include "../streams/voxel_block_request.h"
#include <core/resource.h>

union VoxelSingleValue {
	uint64_t i;
	float f;
};

// Provides access to read-only generated voxels.
// Must be implemented in a multi-thread-safe way.
class VoxelGenerator : public Resource {
	GDCLASS(VoxelGenerator, Resource)
public:
	VoxelGenerator();

	struct Result {
		// Used for block optimization when LOD is used.
		// If this is `false`, more precise data may be found if a lower LOD index is requested.
		// If `true`, any block below this LOD are considered to not bring more details or will be the same.
		// This allows to reduce the number of blocks to load when LOD is used.
		bool max_lod_hint = false;
	};

	virtual Result generate_block(VoxelBlockRequest &input);
	// TODO Single sample

	virtual bool supports_single_generation() const { return false; }

	// TODO Not sure if it's a good API regarding performance
	virtual VoxelSingleValue generate_single(Vector3i pos, unsigned int channel);

	// virtual void generate_series(
	// 		Span<const Vector3> positions,
	// 		Span<const uint8_t> channels,
	// 		Span<Span<VoxelSingleValue>> out_values);

	// Declares the channels this generator will use
	virtual int get_used_channels_mask() const;

protected:
	static void _bind_methods();

	void _b_generate_block(Ref<VoxelBuffer> out_buffer, Vector3 origin_in_voxels, int lod);
};

#endif // VOXEL_GENERATOR_H
