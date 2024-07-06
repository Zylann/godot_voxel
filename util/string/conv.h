#ifndef ZN_STRING_CONV_H
#define ZN_STRING_CONV_H

#include "../containers/span.h"
#include <cstdint>
#include <string_view>

namespace zylann {

static constexpr unsigned int MAX_INT32_CHAR_COUNT_BASE10 = 11; // -2147483647

// Converts integer to characters. Returns the number of characters written.
unsigned int int32_to_string_base10(const int32_t x, Span<uint8_t> s);

// Converts characters to integer. Returns the number of characters read, or -1 in case of failure.
int string_base10_to_int32(std::string_view s, int32_t &out_x);

} // namespace zylann

#endif // ZN_STRING_CONV_H
