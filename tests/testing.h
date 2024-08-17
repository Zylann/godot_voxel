#ifndef ZN_TEST_UTIL_H
#define ZN_TEST_UTIL_H

#include "../util/godot/core/string.h"

// TODO These should actually make the test return and only then fail. This is to allow things like test directories to
// be cleaned up

#define ZN_TEST_ASSERT(m_cond)                                                                                         \
	if ((m_cond) == false) {                                                                                           \
		_err_print_error(FUNCTION_STR, __FILE__, __LINE__, "FATAL: Condition \"" #m_cond "\" is false.");              \
		GENERATE_TRAP();                                                                                               \
	}

#define ZN_TEST_ASSERT_V(m_cond, m_retval)                                                                             \
	if ((m_cond) == false) {                                                                                           \
		_err_print_error(FUNCTION_STR, __FILE__, __LINE__, "FATAL: Condition \"" #m_cond "\" is false.");              \
		GENERATE_TRAP();                                                                                               \
	}

#define ZN_TEST_ASSERT_MSG(m_cond, m_msg)                                                                              \
	if ((m_cond) == false) {                                                                                           \
		_err_print_error(FUNCTION_STR, __FILE__, __LINE__, "FATAL: Condition \"" #m_cond "\" is false. " #m_msg);      \
		GENERATE_TRAP();                                                                                               \
	}

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

#endif // ZN_TEST_UTIL_H
