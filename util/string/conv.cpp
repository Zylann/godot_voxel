#include "conv.h"
#include "../io/log.h"
#include "format.h"
#include <charconv>

#define USE_STD

namespace zylann {

unsigned int int32_to_string_base10(const int32_t x, Span<uint8_t> s) {
#ifdef USE_STD
	Span<char> cs = s.reinterpret_cast_to<char>();
	char *begin = cs.data();
	char *end = cs.data() + cs.size();
	const std::to_chars_result res = std::to_chars(begin, end, x, 10);
	ZN_ASSERT_MSG(
			res.ec == std::errc(),
			format("Can't convert integer to string, error {}", std::make_error_code(res.ec).message())
	);
	return res.ptr - begin;
#else
	const unsigned int base = 10;

	ZN_ASSERT(s.size() >= 1);
	unsigned int nchars = 1;

	uint32_t ux;
	if (x < 0) {
		s[0] = '-';
		++nchars;
		ux = -x;
	} else {
		ux = x;
	}
	uint32_t tx = ux;
	while (tx >= base) {
		tx /= base;
		++nchars;
	}

	ZN_ASSERT(nchars <= s.size());

	unsigned int pos = nchars;
	while (true) {
		--pos;
		s[pos] = '0' + (ux % base);
		ux /= base;
		if (ux == 0) {
			break;
		}
	}

	return nchars;
#endif
}

int string_base10_to_int32(std::string_view s, int32_t &out_x) {
#ifdef USE_STD
	const std::from_chars_result res = std::from_chars(s.data(), s.data() + s.size(), out_x);
	ZN_ASSERT_RETURN_V_MSG(
			res.ec == std::errc(),
			-1,
			format("Can't convert string \"{}\" to int32, error {}", s, std::make_error_code(res.ec).message())
	);
	return res.ptr - s.data();
#else
	unsigned int pos = 0;
	const int base = 10;

	const bool negative = s[pos] == '-';
	if (negative) {
		++pos;
	}
	int64_t x = 0;
	while (pos < s.size()) {
		char c = s[pos];
		if (c >= '0' && c <= '9') {
			x = x * base + (c - '0');
			if ((negative && -x < std::numeric_limits<int32_t>::min()) ||
				(!negative && x > std::numeric_limits<int32_t>::max())) {
				ZN_PRINT_ERROR(format("Can't parse \"{}\" to a 32-bit integer", s));
				return -1;
			}
			++pos;
		} else {
			break;
		}
	}

	out_x = negative ? -x : x;
	return pos;
#endif
}

} // namespace zylann
