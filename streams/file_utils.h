#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include "../util/godot/classes/file.h"
#include "../util/math/vector3i.h"

namespace zylann {

inline Vector3i get_vec3u8(FileAccess &f) {
	Vector3i v;
	v.x = f.get_8();
	v.y = f.get_8();
	v.z = f.get_8();
	return v;
}

inline void store_vec3u8(FileAccess &f, const Vector3i v) {
	f.store_8(v.x);
	f.store_8(v.y);
	f.store_8(v.z);
}

inline Vector3i get_vec3u32(FileAccess &f) {
	Vector3i v;
	v.x = f.get_32();
	v.y = f.get_32();
	v.z = f.get_32();
	return v;
}

inline void store_vec3u32(FileAccess &f, const Vector3i v) {
	f.store_32(v.x);
	f.store_32(v.y);
	f.store_32(v.z);
}

enum FileResult {
	FILE_OK = 0,
	FILE_CANT_OPEN,
	FILE_DOES_NOT_EXIST,
	FILE_UNEXPECTED_EOF,
	FILE_INVALID_MAGIC,
	FILE_INVALID_VERSION,
	FILE_INVALID_DATA
};

const char *to_string(FileResult res);
FileResult check_magic_and_version(
		FileAccess &f, uint8_t expected_version, const char *expected_magic, uint8_t &out_version);

namespace voxel {
// Specific to voxel because it uses a global lock found only in VoxelServer
Error check_directory_created_using_file_locker(const std::string &directory_path);
} // namespace voxel

void insert_bytes(FileAccess &f, size_t count, size_t temp_chunk_size = 512);

} // namespace zylann

#endif // FILE_UTILS_H
