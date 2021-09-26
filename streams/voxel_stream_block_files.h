#ifndef VOXEL_STREAM_BLOCK_FILES_H
#define VOXEL_STREAM_BLOCK_FILES_H

#include "file_utils.h"
#include "voxel_block_serializer.h"
#include "voxel_stream.h"

class FileAccess;

// Loads and saves blocks to the filesystem, under a directory.
// Each block gets its own file, which may produce a lot of them, but it makes it simple to implement.
// This is a naive implementation and may be very slow in practice, so maybe it will be removed in the future.
class VoxelStreamBlockFiles : public VoxelStream {
	GDCLASS(VoxelStreamBlockFiles, VoxelStream)
public:
	VoxelStreamBlockFiles();

	Result emerge_block(VoxelBufferInternal &out_buffer, Vector3i origin_in_voxels, int lod) override;
	void immerge_block(VoxelBufferInternal &buffer, Vector3i origin_in_voxels, int lod) override;

	int get_used_channels_mask() const override;

	String get_directory() const;
	void set_directory(String dirpath);

	int get_block_size_po2() const override;

protected:
	static void _bind_methods();

private:
	VoxelFileResult save_meta();
	VoxelFileResult load_meta();
	VoxelFileResult load_or_create_meta();
	String get_block_file_path(const Vector3i &block_pos, unsigned int lod) const;
	Vector3i get_block_position(const Vector3i &origin_in_voxels) const;

	static thread_local VoxelBlockSerializerInternal _block_serializer;

	String _directory_path;

	struct Meta {
		uint8_t version = -1;
		uint8_t lod_count = 0;
		uint8_t block_size_po2 = 0; // How many voxels in a block
		FixedArray<VoxelBufferInternal::Depth, VoxelBufferInternal::MAX_CHANNELS> channel_depths;
	};

	Meta _meta;
	bool _meta_loaded = false;
	bool _meta_saved = false;
};

#endif // VOXEL_STREAM_BLOCK_FILES_H
