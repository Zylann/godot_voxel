#ifndef ZN_GODOT_FILE_H
#define ZN_GODOT_FILE_H

#if defined(ZN_GODOT)
#include <core/io/file_access.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/global_constants.hpp> // For `Error`
using namespace godot;
#endif

#include "../../span.h"
#include "../funcs.h"

namespace zylann {

inline Ref<FileAccess> open_file(const String path, FileAccess::ModeFlags mode_flags, Error &out_error) {
#if defined(ZN_GODOT)
	return FileAccess::open(path, mode_flags, &out_error);
#elif defined(ZN_GODOT_EXTENSION)
	Ref<FileAccess> file = FileAccess::open(path, mode_flags);
	out_error = FileAccess::get_open_error();
	if (out_error != godot::OK) {
		return Ref<FileAccess>();
	} else {
		return file;
	}
#endif
}

inline uint64_t get_buffer(FileAccess &f, Span<uint8_t> dst) {
#if defined(ZN_GODOT)
	return f.get_buffer(dst.data(), dst.size());
#elif defined(ZN_GODOT_EXTENSION)
	PackedByteArray bytes = f.get_buffer(dst.size());
	copy_to(dst, bytes);
	return bytes.size();
#endif
}

inline void store_buffer(FileAccess &f, Span<const uint8_t> src) {
#if defined(ZN_GODOT)
	f.store_buffer(src.data(), src.size());
#elif defined(ZN_GODOT_EXTENSION)
	PackedByteArray bytes;
	copy_to(bytes, src);
	f.store_buffer(bytes);
#endif
}

inline String get_as_text(FileAccess &f) {
#if defined(ZN_GODOT)
	return f.get_as_utf8_string();
#elif defined(ZN_GODOT_EXTENSION)
	return f.get_as_text();
#endif
}

} // namespace zylann

#endif // ZN_GODOT_FILE_H
