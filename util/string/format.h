#ifndef ZN_STRING_FORMAT_H
#define ZN_STRING_FORMAT_H

#ifdef DEV_ENABLED
#include "../containers/span.h"
#endif
#include "std_string.h"
#include "std_stringstream.h"
#include <sstream>
#include <string_view>

namespace zylann {
namespace strfuncs_detail {

// Not a big implementation, only what I need.
template <typename T>
std::string_view consume_next_format_placeholder(std::string_view fmt, StdStringStream &ss, const T &a) {
	const size_t pi = fmt.find("{}");
	if (pi == std::string_view::npos) {
		// Too many arguments supplied?
		ss << fmt << " [...]";
		return "";
	}
	ss << fmt.substr(0, pi);
	ss << a;
	return fmt.substr(pi + 2);
}

template <typename T0>
std::string_view consume_placeholders(std::string_view fmt, StdStringStream &ss, const T0 &a0) {
	return consume_next_format_placeholder(fmt, ss, a0);
}

template <typename T0, typename... TN>
std::string_view consume_placeholders(std::string_view fmt, StdStringStream &ss, const T0 &a0, const TN &...an) {
	fmt = consume_next_format_placeholder(fmt, ss, a0);
	return consume_placeholders(fmt, ss, an...);
}

} // namespace strfuncs_detail

template <typename... TN>
StdString format(std::string_view fmt, const TN &...an) {
	StdStringStream ss;
	fmt = strfuncs_detail::consume_placeholders(fmt, ss, an...);
	ss << fmt;
	return ss.str();
}

#ifdef DEV_ENABLED
StdString to_hex_table(Span<const uint8_t> data);
#endif

} // namespace zylann

#endif // ZN_STRING_FORMAT_H
