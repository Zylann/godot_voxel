#include "file_utils.h"
#include "../engine/voxel_engine.h"
#include "../util/godot/classes/directory.h"

namespace zylann {

const char *to_string(FileResult res) {
	switch (res) {
		case FILE_OK:
			return "OK";
		case FILE_CANT_OPEN:
			return "Can't open";
		case FILE_DOES_NOT_EXIST:
			return "Doesn't exist";
		case FILE_UNEXPECTED_EOF:
			return "Unexpected end of file";
		case FILE_INVALID_MAGIC:
			return "Invalid magic";
		case FILE_INVALID_VERSION:
			return "Invalid version";
		case FILE_INVALID_DATA:
			return "Invalid data";
		default:
			CRASH_NOW();
			return nullptr;
	}
}

FileResult check_magic_and_version(
		FileAccess &f, uint8_t expected_version, const char *expected_magic, uint8_t &out_version) {
	uint8_t magic[5] = { '\0' };
	const size_t count = get_buffer(f, Span<uint8_t>(magic, 4));
	if (count != 4) {
		return FILE_UNEXPECTED_EOF;
	}
	for (int i = 0; i < 4; ++i) {
		if (magic[i] != expected_magic[i]) {
			return FILE_INVALID_MAGIC;
		}
	}

	out_version = f.get_8();
	if (out_version != expected_version) {
		return FILE_INVALID_VERSION;
	}

	return FILE_OK;
}

Error check_directory_created(const std::string &p_directory_path) {
	const String directory_path(p_directory_path.c_str());

	Ref<DirAccess> d = open_directory(directory_path);

	if (d.is_null()) {
		ERR_PRINT("Could not access to filesystem");
		return ERR_FILE_CANT_OPEN;
	}

	if (!directory_exists(**d, directory_path)) {
		// Create if not exist
		const Error err = d->make_dir_recursive(directory_path);
		if (err != OK) {
			ERR_PRINT("Could not create directory");
			return err;
		}
	}

	return OK;
}

namespace voxel {
Error check_directory_created_using_file_locker(const std::string &directory_path) {
	VoxelFileLockerWrite file_wlock(directory_path);
	return check_directory_created(directory_path);
}
} // namespace voxel

// TODO Write tests

// Makes the file bigger to move the half from the current position further,
// so that it makes room for the specified amount of bytes.
// The new allocated "free" bytes have undefined values, which may be later overwritten by the caller anyways.
void insert_bytes(FileAccess &f, size_t count, size_t temp_chunk_size) {
	CRASH_COND(temp_chunk_size == 0);

	const size_t prev_file_len = f.get_length();
	const size_t insert_pos = f.get_position();

	// Make the file larger
	f.seek(prev_file_len);
	for (size_t i = 0; i < count; ++i) {
		f.store_8(0);
	}

	const size_t initial_bytes_to_move = prev_file_len - insert_pos;
	size_t bytes_to_move = initial_bytes_to_move;
	std::vector<uint8_t> temp;
	size_t src_pos = prev_file_len;
	size_t dst_pos = f.get_length();

	// Copy chunks of the file at a later position, from last to first.
	// The last copied chunk can be smaller.
	while (bytes_to_move > 0) {
		size_t chunk_size = bytes_to_move >= temp_chunk_size ? temp_chunk_size : bytes_to_move;
		src_pos -= chunk_size;
		dst_pos -= chunk_size;
		temp.resize(chunk_size);
		f.seek(src_pos);
		const size_t read_size = get_buffer(f, to_span(temp));
		CRASH_COND(read_size != temp.size());
		f.seek(dst_pos);
		store_buffer(f, to_span(temp));
		bytes_to_move -= temp.size();
		CRASH_COND(bytes_to_move >= initial_bytes_to_move);
	}
}

} // namespace zylann
