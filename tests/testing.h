#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include "../util/span.h"

namespace zylann::testing {

// Utilities for testing

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

#endif // TEST_UTIL_H
