#ifndef ZN_GODOT_DIRECTORY_H
#define ZN_GODOT_DIRECTORY_H

#if defined(ZN_GODOT)
#include <core/io/dir_access.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/directory.hpp>
using namespace godot;
#endif

namespace zylann {

#if defined(ZN_GODOT)
typedef DirAccess GodotDirectory;
#elif defined(ZN_GODOT_EXTENSION)
typedef Directory GodotDirectory;
#endif

inline Ref<GodotDirectory> open_directory(const String &directory_path) {
#if defined(ZN_GODOT)
	return DirAccess::create_for_path(directory_path);
#elif defined(ZN_GODOT_EXTENSION)
	Ref<Directory> dir;
	dir.instantiate();
	const Error err = dir->open(directory_path);
	if (err != OK) {
		return Ref<Directory>();
	} else {
		return dir;
	}
#endif
}

inline bool directory_exists(GodotDirectory &dir, const String &directory_path) {
#if defined(ZN_GODOT)
	return dir.exists(directory_path);
#elif defined(ZN_GODOT_EXTENSION)
	// Why this function is not `const`, I wonder
	return dir.dir_exists(directory_path);
#endif
}

} // namespace zylann

#endif // ZN_GODOT_DIRECTORY_H
