#ifndef VOXEL_STREAM_SCRIPT_H
#define VOXEL_STREAM_SCRIPT_H

#include "voxel_stream.h"

// Provides access to a source of paged voxel data, which may load and save.
// Must be implemented in a multi-thread-safe way.
// If you are looking for a more specialized API to generate voxels, use VoxelGenerator.
class VoxelStreamScript : public VoxelStream {
	GDCLASS(VoxelStreamScript, VoxelStream)
public:
	Result emerge_block(VoxelBufferInternal &out_buffer, Vector3i origin_in_voxels, int lod) override;
	void immerge_block(VoxelBufferInternal &buffer, Vector3i origin_in_voxels, int lod) override;

	int get_used_channels_mask() const override;

protected:
	static void _bind_methods();
};

#endif // VOXEL_STREAM_SCRIPT_H
