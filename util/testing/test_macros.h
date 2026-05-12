#ifndef ZN_TEST_MACROS_H
#define ZN_TEST_MACROS_H

#include "../errors.h"

// TODO These should actually make the test return and only then fail. This is to allow things like test directories to
// be cleaned up

#define ZN_TEST_ASSERT(m_cond)                                                                                         \
	if ((m_cond) == false) {                                                                                           \
		zylann::print_error("FATAL: Condition \"" #m_cond "\" is false.", FUNCTION_STR, __FILE__, __LINE__);           \
		GENERATE_TRAP();                                                                                               \
	}

#define ZN_TEST_ASSERT_V(m_cond, m_retval)                                                                             \
	if ((m_cond) == false) {                                                                                           \
		zylann::print_error("FATAL: Condition \"" #m_cond "\" is false.", FUNCTION_STR, __FILE__, __LINE__);           \
		GENERATE_TRAP();                                                                                               \
	}

#define ZN_TEST_ASSERT_MSG(m_cond, m_msg)                                                                              \
	if ((m_cond) == false) {                                                                                           \
		zylann::print_error("FATAL: Condition \"" #m_cond "\" is false. " #m_msg, FUNCTION_STR, __FILE__, __LINE__);   \
		GENERATE_TRAP();                                                                                               \
	}

#endif // ZN_TEST_MACROS_H
