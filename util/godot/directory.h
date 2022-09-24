#ifndef ZN_GODOT_DIRECTORY_H
#define ZN_GODOT_DIRECTORY_H

#if defined(ZN_GODOT)
#include <core/io/dir_access.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/dir_access.hpp>
using namespace godot;
#endif

namespace zylann {

inline Ref<DirAccess> open_directory(const String &directory_path) {
#if defined(ZN_GODOT)
	return DirAccess::create_for_path(directory_path);
#elif defined(ZN_GODOT_EXTENSION)
	Ref<DirAccess> dir = DirAccess::open(directory_path);
	const Error err = DirAccess::get_open_error();
	if (err != OK) {
		return Ref<DirAccess>();
	} else {
		return dir;
	}
#endif
}

inline bool directory_exists(DirAccess &dir, const String &directory_path) {
#if defined(ZN_GODOT)
	return dir.exists(directory_path);
#elif defined(ZN_GODOT_EXTENSION)
	// Why this function is not `const`, I wonder
	return dir.dir_exists(directory_path);
#endif
}

} // namespace zylann

#endif // ZN_GODOT_DIRECTORY_H
