#ifndef VOXEL_GENERATOR_H
#define VOXEL_GENERATOR_H

#include <core/io/resource.h>
#include <core/math/vector3i.h>
#include <core/variant/typed_array.h>

namespace zylann::voxel {

class VoxelBufferInternal;

namespace gd {
class VoxelBuffer;
}

// Non-encoded, generic voxel value.
// (Voxels stored inside VoxelBuffers are encoded to take less space)
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

	struct VoxelQueryData {
		VoxelBufferInternal &voxel_buffer;
		Vector3i origin_in_voxels;
		uint32_t lod;
	};

	virtual Result generate_block(VoxelQueryData &input);

	virtual bool supports_single_generation() const {
		return false;
	}

	// TODO Not sure if it's a good API regarding performance
	virtual VoxelSingleValue generate_single(Vector3i pos, unsigned int channel);

	// virtual void generate_series(
	// 		Span<const Vector3> positions,
	// 		Span<const uint8_t> channels,
	// 		Span<Span<VoxelSingleValue>> out_values);

	// Declares the channels this generator will use
	virtual int get_used_channels_mask() const;

#ifdef TOOLS_ENABLED
	virtual void get_configuration_warnings(TypedArray<String> &out_warnings) const {}
#endif

protected:
	static void _bind_methods();

	void _b_generate_block(Ref<gd::VoxelBuffer> out_buffer, Vector3 origin_in_voxels, int lod);
};

} // namespace zylann::voxel

#endif // VOXEL_GENERATOR_H
