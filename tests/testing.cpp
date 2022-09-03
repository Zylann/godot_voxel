#include "testing.h"

#include <core/io/dir_access.h>
#include <core/io/file_access.h>

namespace zylann::testing {

constexpr char *DEFAULT_TEST_DATA_DIRECTORY = "zylann_testing_dir";

bool create_empty_file(String fpath) {
	if (!FileAccess::exists(fpath)) {
		Ref<FileAccess> f = FileAccess::open(fpath, FileAccess::WRITE);
		if (f.is_valid()) {
			f->store_line("");
		} else {
			ERR_PRINT("Failed to create file " + fpath);
			return false;
		}
	}
	return true;
}

bool remove_dir_if_exists(const char *p_dirpath) {
	String dirpath = p_dirpath;
	// If this is an empty string we could end up deleting the whole project, don't risk that
	ERR_FAIL_COND_V(dirpath.is_empty(), false);

	if (DirAccess::exists(dirpath)) {
		Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		ERR_FAIL_COND_V(da.is_null(), false);

		String prev_dir = da->get_current_dir();

		// Note, this does not change the working directory of the application.
		// Very important to do that first, otherwise `erase_contents_recursive` would erase the whole project
		const Error cd_err = da->change_dir(dirpath);
		ERR_FAIL_COND_V(cd_err != OK, false);

		// `remove` fails if the directory is not empty
		const Error contents_remove_err = da->erase_contents_recursive();
		ERR_FAIL_COND_V(contents_remove_err != OK, false);
		// OS::get_singleton()->move_to_trash(dirpath); // ?

		const Error cd_err2 = da->change_dir(prev_dir);
		ERR_FAIL_COND_V(cd_err2 != OK, false);

		const Error remove_err = da->remove(dirpath);
		ERR_FAIL_COND_V(remove_err != OK, false);
	}

	return true;
}

bool create_clean_dir(const char *dirpath) {
	ERR_FAIL_COND_V(dirpath == nullptr, false);
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	ERR_FAIL_COND_V(da.is_null(), false);

	ERR_FAIL_COND_V(!remove_dir_if_exists(dirpath), false);

	const Error err = da->make_dir(dirpath);
	ERR_FAIL_COND_V(err != OK, false);

	ERR_FAIL_COND_V(!create_empty_file(String(dirpath).path_join(".gdignore")), false);

	return true;
}

TestDirectory::TestDirectory() {
	ERR_FAIL_COND(!create_clean_dir(DEFAULT_TEST_DATA_DIRECTORY));
	_valid = true;
}

TestDirectory::~TestDirectory() {
	ERR_FAIL_COND(!remove_dir_if_exists(DEFAULT_TEST_DATA_DIRECTORY));
}

String TestDirectory::get_path() const {
	return DEFAULT_TEST_DATA_DIRECTORY;
}

} //namespace zylann::testing
