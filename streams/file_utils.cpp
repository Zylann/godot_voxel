#include "file_utils.h"

const char *to_string(VoxelFileResult res) {
	switch (res) {
		case VOXEL_FILE_OK:
			return "OK";
		case VOXEL_FILE_CANT_OPEN:
			return "Can't open";
		case VOXEL_FILE_UNEXPECTED_EOF:
			return "Unexpected end of file";
		case VOXEL_FILE_INVALID_MAGIC:
			return "Invalid magic";
		case VOXEL_FILE_INVALID_VERSION:
			return "Invalid version";
		case VOXEL_FILE_INVALID_DATA:
			return "Invalid data";
		default:
			CRASH_NOW();
			return nullptr;
	}
}

VoxelFileResult check_magic_and_version(FileAccess *f, uint8_t expected_version, const char *expected_magic, uint8_t &out_version) {
	uint8_t magic[5] = { '\0' };
	int count = f->get_buffer(magic, 4);
	if (count != 4) {
		return VOXEL_FILE_UNEXPECTED_EOF;
	}
	for (int i = 0; i < 4; ++i) {
		if (magic[i] != expected_magic[i]) {
			return VOXEL_FILE_INVALID_MAGIC;
		}
	}

	out_version = f->get_8();
	if (out_version != expected_version) {
		return VOXEL_FILE_INVALID_VERSION;
	}

	return VOXEL_FILE_OK;
}

Error check_directory_created(const String &directory_path) {
	DirAccess *d = DirAccess::create_for_path(directory_path);

	if (d == nullptr) {
		ERR_PRINT("Could not access to filesystem");
		return ERR_FILE_CANT_OPEN;
	}

	if (!d->exists(directory_path)) {
		// Create if not exist
		Error err = d->make_dir_recursive(directory_path);
		if (err != OK) {
			ERR_PRINT("Could not create directory");
			memdelete(d);
			return err;
		}
	}

	memdelete(d);
	return OK;
}
