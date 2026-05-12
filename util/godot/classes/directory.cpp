#include "directory.h"
#include "../../containers/std_vector.h"

namespace zylann::godot {

#ifdef TOOLS_ENABLED

Error erase_directory_contents_recursive(DirAccess &da) {
#ifdef ZN_GODOT
	return da.erase_contents_recursive();

#elif ZN_GODOT_EXTENSION
	// Ported from https://github.com/godotengine/godot/blob/master/core/io/dir_access.cpp#L78
	// https://github.com/godotengine/godot-proposals/issues/11598

	List<String> dirs;
	List<String> files;

	da.list_dir_begin();
	String n = da.get_next();
	while (!n.is_empty()) {
		if (n != "." && n != "..") {
			if (da.current_is_dir() && !da.is_link(n)) {
				dirs.push_back(n);
			} else {
				files.push_back(n);
			}
		}

		n = da.get_next();
	}

	da.list_dir_end();

	for (const String &E : dirs) {
		Error err = da.change_dir(E);
		if (err == OK) {
			err = erase_directory_contents_recursive(da);
			if (err) {
				da.change_dir("..");
				return err;
			}
			err = da.change_dir("..");
			if (err) {
				return err;
			}
			err = da.remove(da.get_current_dir().path_join(E));
			if (err) {
				return err;
			}
		} else {
			return err;
		}
	}

	for (const String &E : files) {
		Error err = da.remove(da.get_current_dir().path_join(E));
		if (err) {
			return err;
		}
	}

	return OK;
#endif
}

#endif

} // namespace zylann::godot
