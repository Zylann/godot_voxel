#include "test_string_funcs.h"
#include "../../util/containers/fixed_array.h"
#include "../../util/string/conv.h"

namespace zylann::tests {

void test_int32_to_string_base10(const int32_t x, std::string_view expected) {
	FixedArray<uint8_t, 64> buffer;
	const unsigned int nchars = int32_to_string_base10(x, to_span(buffer));

	const unsigned int expected_nchars = expected.size();
	ZN_ASSERT(nchars == expected_nchars);

	for (unsigned int i = 0; i < expected_nchars; ++i) {
		ZN_ASSERT(buffer[i] == static_cast<uint8_t>(expected[i]));
	}
}

void test_int32_to_string_base10() {
	test_int32_to_string_base10(0, "0");
	test_int32_to_string_base10(1, "1");
	test_int32_to_string_base10(-1, "-1");
	test_int32_to_string_base10(42, "42");
	test_int32_to_string_base10(123456789, "123456789");
	test_int32_to_string_base10(-123456789, "-123456789");
	test_int32_to_string_base10(std::numeric_limits<int32_t>::min(), "-2147483648");
	test_int32_to_string_base10(std::numeric_limits<int32_t>::max(), "2147483647");
}

void test_string_base10_to_int32(const char *src, const int32_t expected, const unsigned int expected_nchars) {
	int32_t x;
	std::string_view src_sv(src);
	const unsigned int nchars = string_base10_to_int32(src_sv, x);
	ZN_ASSERT(expected_nchars <= src_sv.size());
	ZN_ASSERT(nchars == expected_nchars);
	ZN_ASSERT(x == expected);
}

void test_string_base10_to_int32() {
	test_string_base10_to_int32("0", 0, 1);
	test_string_base10_to_int32("1", 1, 1);
	test_string_base10_to_int32("-1", -1, 2);
	test_string_base10_to_int32("42", 42, 2);
	test_string_base10_to_int32("42abc", 42, 2);
	test_string_base10_to_int32("-42abc", -42, 3);
	test_string_base10_to_int32("42,43", 42, 2);
	test_string_base10_to_int32("123456789", 123456789, 9);
	test_string_base10_to_int32("-123456789", -123456789, 10);
	test_string_base10_to_int32("-2147483648", std::numeric_limits<int32_t>::min(), 11);
	test_string_base10_to_int32("2147483647", std::numeric_limits<int32_t>::max(), 10);
}

} // namespace zylann::tests
