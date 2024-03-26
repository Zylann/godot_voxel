#include "file_utils.h"
#include "../containers/std_vector.h"
#include "classes/directory.h"

namespace zylann::godot {

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
		case FILE_INVALID_DATA:
			return "Invalid data";
		default:
			CRASH_NOW();
			return nullptr;
	}
}

Error check_directory_created(const String &p_directory_path) {
	Ref<DirAccess> d = open_directory(p_directory_path);

	if (d.is_null()) {
		ERR_PRINT("Could not access to filesystem");
		return ERR_FILE_CANT_OPEN;
	}

	if (!directory_exists(**d, p_directory_path)) {
		// Create if not exist
		const Error err = d->make_dir_recursive(p_directory_path);
		if (err != OK) {
			ERR_PRINT("Could not create directory");
			return err;
		}
	}

	return OK;
}

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
	StdVector<uint8_t> temp;
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

} // namespace zylann::godot
