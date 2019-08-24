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

	int get_block_size_po2() const override;

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
		uint8_t block_size_po2 = 0; // How many voxels in a block
	};

	Meta _meta;
	bool _meta_loaded = false;
	bool _meta_saved = false;
};

#endif // VOXEL_STREAM_BLOCK_FILES_H
