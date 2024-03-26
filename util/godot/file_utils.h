#ifndef ZN_GODOT_FILE_UTILS_H
#define ZN_GODOT_FILE_UTILS_H

#include "../math/vector3i.h"
#include "classes/file_access.h"
#include "core/string.h"

namespace zylann::godot {

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

enum FileResult { //
	FILE_OK = 0,
	FILE_CANT_OPEN,
	FILE_DOES_NOT_EXIST,
	FILE_UNEXPECTED_EOF,
	FILE_INVALID_DATA
};

const char *to_string(FileResult res);

Error check_directory_created(const String &p_directory_path);

void insert_bytes(FileAccess &f, size_t count, size_t temp_chunk_size = 512);

} // namespace zylann::godot

#endif // ZN_GODOT_FILE_UTILS_H
