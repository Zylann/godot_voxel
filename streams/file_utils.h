#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include "../util/math/vector3i.h"
#include <core/os/dir_access.h>
#include <core/os/file_access.h>

inline Vector3i get_vec3u8(FileAccess *f) {
	Vector3i v;
	v.x = f->get_8();
	v.y = f->get_8();
	v.z = f->get_8();
	return v;
}

inline void store_vec3u8(FileAccess *f, const Vector3i v) {
	f->store_8(v.x);
	f->store_8(v.y);
	f->store_8(v.z);
}

inline Vector3i get_vec3u32(FileAccess *f) {
	Vector3i v;
	v.x = f->get_32();
	v.y = f->get_32();
	v.z = f->get_32();
	return v;
}

inline void store_vec3u32(FileAccess *f, const Vector3i v) {
	f->store_32(v.x);
	f->store_32(v.y);
	f->store_32(v.z);
}

enum VoxelFileResult {
	VOXEL_FILE_OK = 0,
	VOXEL_FILE_CANT_OPEN,
	VOXEL_FILE_DOES_NOT_EXIST,
	VOXEL_FILE_UNEXPECTED_EOF,
	VOXEL_FILE_INVALID_MAGIC,
	VOXEL_FILE_INVALID_VERSION,
	VOXEL_FILE_INVALID_DATA
};

const char *to_string(VoxelFileResult res);
VoxelFileResult check_magic_and_version(FileAccess *f, uint8_t expected_version, const char *expected_magic, uint8_t &out_version);
Error check_directory_created(const String &directory_path);

// TODO Wrap other stuff in that
namespace VoxelFileUtils {

void insert_bytes(FileAccess *f, size_t count, size_t temp_chunk_size = 512);

} // namespace VoxelFileUtils

#endif // FILE_UTILS_H
