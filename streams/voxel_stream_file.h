#ifndef VOXEL_STREAM_FILE_H
#define VOXEL_STREAM_FILE_H

#include "voxel_stream.h"

// Loads and saves blocks to the filesystem.
// If a block is not found, a fallback stream can be used (usually to generate the block).
// Look at subclasses of this for a specific format.
class VoxelStreamFile : public VoxelStream {
	GDCLASS(VoxelStreamFile, VoxelStream)
public:
	Ref<VoxelStream> get_fallback_stream() const;
	void set_fallback_stream(Ref<VoxelStream> stream);

protected:
	static void _bind_methods();

	void emerge_block_fallback(Ref<VoxelBuffer> out_buffer, Vector3i origin_in_voxels, int lod);

private:
	Ref<VoxelStream> _fallback_stream;
};

#endif // VOXEL_STREAM_FILE_H
