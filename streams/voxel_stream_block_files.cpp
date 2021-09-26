#include "voxel_stream_block_files.h"
#include "../server/voxel_server.h"

#include <core/os/dir_access.h>
#include <core/os/file_access.h>

namespace {
const uint8_t FORMAT_VERSION = 1;
const char *FORMAT_META_MAGIC = "VXBM";
const char *FORMAT_BLOCK_MAGIC = "VXB_";
const char *META_FILE_NAME = "meta.vxbm";
const char *BLOCK_FILE_EXTENSION = ".vxb";
} // namespace

thread_local VoxelBlockSerializerInternal VoxelStreamBlockFiles::_block_serializer;

VoxelStreamBlockFiles::VoxelStreamBlockFiles() {
	// Defaults
	_meta.block_size_po2 = 4;
	_meta.lod_count = 1;
	_meta.version = FORMAT_VERSION;
	_meta.channel_depths.fill(VoxelBufferInternal::DEFAULT_CHANNEL_DEPTH);
}

// TODO Have configurable block size

VoxelStream::Result VoxelStreamBlockFiles::emerge_block(
		VoxelBufferInternal &out_buffer, Vector3i origin_in_voxels, int lod) {
	//
	if (_directory_path.empty()) {
		return RESULT_BLOCK_NOT_FOUND;
	}

	if (!_meta_loaded) {
		if (load_meta() != VOXEL_FILE_OK) {
			return RESULT_ERROR;
		}
	}

	CRASH_COND(!_meta_loaded);

	const Vector3i block_size(1 << _meta.block_size_po2);

	ERR_FAIL_COND_V(lod >= _meta.lod_count, RESULT_ERROR);
	ERR_FAIL_COND_V(block_size != out_buffer.get_size(), RESULT_ERROR);

	Vector3i block_pos = get_block_position(origin_in_voxels) >> lod;
	String file_path = get_block_file_path(block_pos, lod);

	FileAccess *f = nullptr;
	VoxelFileLockerRead file_rlock(file_path);
	{
		Error err;
		f = FileAccess::open(file_path, FileAccess::READ, &err);
		// Had to add ERR_FILE_CANT_OPEN because that's what Godot actually returns when the file doesn't exist...
		if (f == nullptr && (err == ERR_FILE_NOT_FOUND || err == ERR_FILE_CANT_OPEN)) {
			return RESULT_BLOCK_NOT_FOUND;
		}
	}

	ERR_FAIL_COND_V(f == nullptr, RESULT_ERROR);

	{
		{
			uint8_t version;
			VoxelFileResult err = check_magic_and_version(f, FORMAT_VERSION, FORMAT_BLOCK_MAGIC, version);
			if (err != VOXEL_FILE_OK) {
				memdelete(f);
				ERR_PRINT(String("Invalid file header: ") + ::to_string(err));
				return RESULT_ERROR;
			}
		}

		// Configure depths, as they currently are only specified in the meta file.
		// Files are expected to contain such depths, and use those in the buffer to know how much data to read.
		for (unsigned int channel_index = 0; channel_index < _meta.channel_depths.size(); ++channel_index) {
			out_buffer.set_channel_depth(channel_index, _meta.channel_depths[channel_index]);
		}

		uint32_t size_to_read = f->get_32();
		if (!_block_serializer.decompress_and_deserialize(f, size_to_read, out_buffer)) {
			ERR_PRINT("Failed to decompress and deserialize");
		}
	}

	f->close();
	memdelete(f);

	return RESULT_BLOCK_FOUND;
}

void VoxelStreamBlockFiles::immerge_block(VoxelBufferInternal &buffer, Vector3i origin_in_voxels, int lod) {
	ERR_FAIL_COND(_directory_path.empty());

	if (!_meta_loaded) {
		// If it's not loaded, always try to load meta file first if it exists already,
		// because we could want to save blocks without reading any
		VoxelFileResult res = load_meta();
		if (res != VOXEL_FILE_OK && res != VOXEL_FILE_CANT_OPEN) {
			// The file is present but there is a problem with it
			String meta_path = _directory_path.plus_file(META_FILE_NAME);
			ERR_PRINT(String("Could not read {0}: {1}").format(varray(meta_path, ::to_string(res))));
			return;
		}
	}

	if (!_meta_saved) {
		// First time we save the meta file, initialize it from the first block format
		for (unsigned int i = 0; i < _meta.channel_depths.size(); ++i) {
			_meta.channel_depths[i] = buffer.get_channel_depth(i);
		}
		VoxelFileResult res = save_meta();
		ERR_FAIL_COND(res != VOXEL_FILE_OK);
	}

	// Check format
	const Vector3i block_size = Vector3i(1 << _meta.block_size_po2);
	ERR_FAIL_COND(buffer.get_size() != block_size);
	for (unsigned int channel_index = 0; channel_index < _meta.channel_depths.size(); ++channel_index) {
		ERR_FAIL_COND(buffer.get_channel_depth(channel_index) != _meta.channel_depths[channel_index]);
	}

	Vector3i block_pos = get_block_position(origin_in_voxels) >> lod;
	String file_path = get_block_file_path(block_pos, lod);

	{
		const Error err = check_directory_created(file_path.get_base_dir());
		ERR_FAIL_COND(err != OK);
	}

	{
		FileAccess *f = nullptr;
		VoxelFileLockerWrite file_wlock(file_path);
		{
			Error err;
			// Create file if not exists, always truncate
			f = FileAccess::open(file_path, FileAccess::WRITE, &err);
		}
		ERR_FAIL_COND(f == nullptr);

		f->store_buffer((uint8_t *)FORMAT_BLOCK_MAGIC, 4);
		f->store_8(FORMAT_VERSION);

		VoxelBlockSerializerInternal::SerializeResult res = _block_serializer.serialize_and_compress(buffer);
		if (!res.success) {
			memdelete(f);
			ERR_PRINT("Failed to save block");
			return;
		}
		f->store_32(res.data.size());
		f->store_buffer(res.data.data(), res.data.size());

		f->close();
		memdelete(f);
	}
}

int VoxelStreamBlockFiles::get_used_channels_mask() const {
	// Assuming all, since that stream can store anything.
	return VoxelBufferInternal::ALL_CHANNELS_MASK;
}

String VoxelStreamBlockFiles::get_directory() const {
	return _directory_path;
}

void VoxelStreamBlockFiles::set_directory(String dirpath) {
	if (_directory_path != dirpath) {
		_directory_path = dirpath;
		_meta_loaded = false;
	}
}

int VoxelStreamBlockFiles::get_block_size_po2() const {
	return _meta.block_size_po2;
}

VoxelFileResult VoxelStreamBlockFiles::save_meta() {
	CRASH_COND(_directory_path.empty());

	// Make sure the directory exists
	{
		Error err = check_directory_created(_directory_path);
		if (err != OK) {
			ERR_PRINT("Could not save meta");
			return VOXEL_FILE_CANT_OPEN;
		}
	}

	String meta_path = _directory_path.plus_file(META_FILE_NAME);

	{
		Error err;
		VoxelFileLockerWrite file_wlock(meta_path);
		FileAccess *f = FileAccess::open(meta_path, FileAccess::WRITE, &err);
		ERR_FAIL_COND_V(f == nullptr, VOXEL_FILE_CANT_OPEN);

		f->store_buffer((uint8_t *)FORMAT_META_MAGIC, 4);
		f->store_8(FORMAT_VERSION);

		f->store_8(_meta.lod_count);
		f->store_8(_meta.block_size_po2);

		for (unsigned int i = 0; i < _meta.channel_depths.size(); ++i) {
			f->store_8(_meta.channel_depths[i]);
		}

		memdelete(f);
	}

	_meta_loaded = true;
	_meta_saved = true;
	return VOXEL_FILE_OK;
}

VoxelFileResult VoxelStreamBlockFiles::load_or_create_meta() {
	VoxelFileResult res = load_meta();
	if (res == VOXEL_FILE_DOES_NOT_EXIST) {
		VoxelFileResult save_result = save_meta();
		ERR_FAIL_COND_V(save_result != VOXEL_FILE_OK, save_result);
		return VOXEL_FILE_OK;
	}
	return res;
}

VoxelFileResult VoxelStreamBlockFiles::load_meta() {
	CRASH_COND(_directory_path.empty());

	String meta_path = _directory_path.plus_file(META_FILE_NAME);

	Meta meta;
	{
		Error open_result;
		VoxelFileLockerRead file_rlock(meta_path);
		FileAccessRef f = FileAccess::open(meta_path, FileAccess::READ, &open_result);
		// Had to add ERR_FILE_CANT_OPEN because that's what Godot actually returns when the file doesn't exist...
		if (!_meta_saved && (open_result == ERR_FILE_NOT_FOUND || open_result == ERR_FILE_CANT_OPEN)) {
			// This is a new terrain, save the meta we have and consider it current
			return VOXEL_FILE_DOES_NOT_EXIST;
		}
		ERR_FAIL_COND_V(!f, VOXEL_FILE_CANT_OPEN);

		VoxelFileResult check_result = check_magic_and_version(f.f, FORMAT_VERSION, FORMAT_META_MAGIC, meta.version);
		if (check_result != VOXEL_FILE_OK) {
			return check_result;
		}

		meta.lod_count = f->get_8();
		meta.block_size_po2 = f->get_8();

		for (unsigned int i = 0; i < meta.channel_depths.size(); ++i) {
			uint8_t depth = f->get_8();
			ERR_FAIL_COND_V(depth >= VoxelBufferInternal::DEPTH_COUNT, VOXEL_FILE_INVALID_DATA);
			meta.channel_depths[i] = (VoxelBufferInternal::Depth)depth;
		}

		ERR_FAIL_COND_V(meta.lod_count < 1 || meta.lod_count > 32, VOXEL_FILE_INVALID_DATA);
		ERR_FAIL_COND_V(meta.block_size_po2 < 1 || meta.block_size_po2 > 8, VOXEL_FILE_INVALID_DATA);
	}

	_meta_loaded = true;
	_meta = meta;
	return VOXEL_FILE_OK;
}

String VoxelStreamBlockFiles::get_block_file_path(const Vector3i &block_pos, unsigned int lod) const {
	// TODO This is probably extremely inefficient, also given the nature of Godot strings

	// Save under a folder, because there could be other kinds of data to store in this terrain
	String path = "blocks/lod";
	path += String::num_uint64(lod);
	path += '/';
	for (unsigned int i = 0; i < 3; ++i) {
		if (block_pos[i] >= 0) {
			path += '+';
		}
		path += String::num_int64(block_pos[i]);
	}
	path += BLOCK_FILE_EXTENSION;
	return _directory_path.plus_file(path);
}

Vector3i VoxelStreamBlockFiles::get_block_position(const Vector3i &origin_in_voxels) const {
	return origin_in_voxels >> _meta.block_size_po2;
}

void VoxelStreamBlockFiles::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_directory", "directory"), &VoxelStreamBlockFiles::set_directory);
	ClassDB::bind_method(D_METHOD("get_directory"), &VoxelStreamBlockFiles::get_directory);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "directory", PROPERTY_HINT_DIR), "set_directory", "get_directory");
}
