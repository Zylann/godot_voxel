#ifndef VOXEL_STREAM_BLOCK_FILES_H
#define VOXEL_STREAM_BLOCK_FILES_H

#include "voxel_stream_file.h"

class FileAccess;

// Loads and saves blocks to the filesystem, under a directory.
// Each block gets its own file, which may produce a lot of them, but it makes it simple to implement.
class VoxelStreamBlockFiles : public VoxelStreamFile {
	GDCLASS(VoxelStreamBlockFiles, VoxelStreamFile)
public:
	VoxelStreamBlockFiles();

	void emerge_block(Ref<VoxelBuffer> out_buffer, Vector3i origin_in_voxels, int lod) override;
	void immerge_block(Ref<VoxelBuffer> buffer, Vector3i origin_in_voxels, int lod) override;

	String get_directory() const;
	void set_directory(String dirpath);

protected:
	static void _bind_methods();

private:
	Error save_meta();
	Error load_meta();
	String get_block_file_path(const Vector3i &block_pos, unsigned int lod) const;
	Vector3i get_block_position(const Vector3i &origin_in_voxels) const;

	String _directory_path;

	struct Meta {
		uint8_t version = -1;
		uint8_t lod_count = 0;
		Vector3i block_size; // How many voxels in a block

		// Non-serialized
		bool loaded = false;
		bool saved = false;
	};

	Meta _meta;
};

#endif // VOXEL_STREAM_BLOCK_FILES_H
