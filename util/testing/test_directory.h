#ifndef ZN_TEST_DIRECTORY_H
#define ZN_TEST_DIRECTORY_H

#include "../godot/core/string.h"

namespace zylann::testing {

// Creates a temporary directory when an instance of this class is created, and removes it after use
class TestDirectory {
public:
	TestDirectory();
	~TestDirectory();

	bool is_valid() const {
		return _valid;
	}

	String get_path() const;

private:
	bool _valid = false;
};

} // namespace zylann::testing

#endif // ZN_TEST_DIRECTORY_H
