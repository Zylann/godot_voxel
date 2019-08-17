#ifndef VOXEL_STREAM_FILE_H
#define VOXEL_STREAM_FILE_H

#include "voxel_block_serializer.h"
#include "voxel_stream.h"

class FileAccess;

// TODO Could be worth integrating with Godot ResourceLoader
// Loads and saves blocks to the filesystem.
// If a block is not found, a fallback stream can be used (usually to generate the block).
// Look at subclasses of this for a specific format.
class VoxelStreamFile : public VoxelStream {
	GDCLASS(VoxelStreamFile, VoxelStream)
public:
	void set_save_fallback_output(bool enabled);
	bool get_save_fallback_output() const;

	Ref<VoxelStream> get_fallback_stream() const;
	void set_fallback_stream(Ref<VoxelStream> stream);

protected:
	static void _bind_methods();

	void emerge_block_fallback(Ref<VoxelBuffer> out_buffer, Vector3i origin_in_voxels, int lod);
	void emerge_blocks_fallback(Vector<BlockRequest> &requests);

	VoxelBlockSerializer _block_serializer;

private:
	Ref<VoxelStream> _fallback_stream;
	bool _save_fallback_output = true;
};

#endif // VOXEL_STREAM_FILE_H
