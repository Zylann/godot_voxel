#include "conv.h"
#include "../io/log.h"
#include "../math/funcs.h"
#include "format.h"
#include <array>
#include <charconv>
#include <cstdio>
#include <system_error>

#define USE_STD

namespace zylann {

template <typename TFloat>
unsigned int float_to_string_null_terminated(const TFloat x, Span<char> s, const unsigned int precision) {
	// Using `%g` alone may strip unnecessary decimals:
	// https://stackoverflow.com/questions/35475425/sprintf-g-specifier-gives-too-few-digits-after-point
	// So we also have to specify the precision.

	const int res = snprintf(s.data(), s.size(), "%.*g", precision, x);
	if (res < 0) {
		ZN_PRINT_ERROR(format("Failed to convert float to string, snprintf returned {}", res));
		return 0;
	}
	// While `snprintf` writes a null-terminator, it is not included in the returned size.
	const unsigned int len_with_null_terminator = res + 1;
	if (len_with_null_terminator > s.size()) {
		ZN_PRINT_ERROR(
				format("Failed to convert float to string, buffer capacity was {} but needed {}",
					   s.size(),
					   len_with_null_terminator)
		);
		return s.size();
	}
	return len_with_null_terminator;
}

template <typename TFloat>
unsigned int float_to_string(const TFloat x, Span<char> s, const unsigned int precision) {
	// This fails to compile on iOS GDExtension prior to 16.3, apparently float versions of `to_chars` aren't supported.
	//
	// char *begin = s.data();
	// char *end = s.data() + s.size();
	// const std::to_chars_result res = std::to_chars(begin, end, x, std::chars_format::general);
	// ZN_ASSERT_MSG(
	// 		res.ec == std::errc(),
	// 		format("Can't convert float to string, error {}", std::make_error_code(res.ec).message())
	// );
	// return res.ptr - begin;

	// `snprintf` always puts a \0` at the end of what it writes, even when the number doesn't fit.
	// But our function's API does not expect that.
	// So we need extra boilerplate...
	std::array<char, max_float_chars_general<TFloat>() + 1> buf;
	Span<char> buf_s(buf.data(), buf.size());
	const unsigned int len_with_null_terminator = float_to_string_null_terminated(x, buf_s, precision);
	if (len_with_null_terminator == 0) {
		return 0;
	}
	const unsigned int len = len_with_null_terminator - 1;
	if (len > s.size()) {
		ZN_PRINT_ERROR(
				format("Failed to convert float to string, buffer capacity was {} but needed {}", s.size(), len)
		);
	}
	const unsigned int rlen = math::min<size_t>(len, s.size());
	buf_s.sub(0, rlen).copy_to(s.sub(0, rlen));
	return rlen;
}

unsigned int float32_to_string(const float x, Span<char> s) {
	return float_to_string<float>(x, s, 8);
}

unsigned int float64_to_string(const double x, Span<char> s) {
	return float_to_string<double>(x, s, 16);
}

template <typename TInt>
unsigned int int_to_string(const TInt x, Span<char> s, const unsigned int base) {
	char *begin = s.data();
	char *end = s.data() + s.size();
	const std::to_chars_result res = std::to_chars(begin, end, x, 10);
	ZN_ASSERT_MSG(
			res.ec == std::errc(),
			format("Can't convert int to string, error {}", std::make_error_code(res.ec).message())
	);
	return res.ptr - begin;
}

unsigned int int64_to_string_base10(const int64_t x, Span<char> s) {
	return int_to_string(x, s, 10);
}

unsigned int int32_to_string_base10(const int32_t x, Span<uint8_t> s) {
#ifdef USE_STD
	Span<char> cs = s.reinterpret_cast_to<char>();
	return int_to_string(x, cs, 10);
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
