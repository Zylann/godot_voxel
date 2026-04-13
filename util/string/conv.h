#ifndef ZN_STRING_CONV_H
#define ZN_STRING_CONV_H

#include "../containers/span.h"
#include "../math/funcs.h"
#include <cstdint>
#include <limits>
#include <string_view>

namespace zylann {

namespace conv_detail {

template <typename T>
constexpr int log10ceil(T num) {
	return num < 10 ? 1 : 1 + log10ceil(num / 10);
}

} // namespace conv_detail

// https://stackoverflow.com/questions/68472720/stdto-chars-minimal-floating-point-buffer-size
template <typename TFloat>
constexpr int max_float_chars_general() {
	return 4 + std::numeric_limits<TFloat>::max_digits10 +
			math::max(2, conv_detail::log10ceil(std::numeric_limits<TFloat>::max_exponent10));
}

static constexpr unsigned int MAX_INT32_CHAR_COUNT_BASE10 = 11; // -2147483647
static constexpr unsigned int MAX_INT64_CHAR_COUNT_BASE10 = 20; // -9223372036854775808

// Converts integer to characters. Returns the number of characters written.
unsigned int int32_to_string_base10(const int32_t x, Span<uint8_t> s);
unsigned int int64_to_string_base10(const int64_t x, Span<char> s);
unsigned int float32_to_string(const float x, Span<char> s);
unsigned int float64_to_string(const double x, Span<char> s);

// Converts characters to integer. Returns the number of characters read, or -1 in case of failure.
int string_base10_to_int32(std::string_view s, int32_t &out_x);

} // namespace zylann

#endif // ZN_STRING_CONV_H
