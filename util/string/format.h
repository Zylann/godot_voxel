#ifndef ZN_STRING_FORMAT_H
#define ZN_STRING_FORMAT_H

#ifdef DEV_ENABLED
#include "../containers/span.h"
#endif
#include "../io/std_string_text_writer.h"
#include "std_string.h"
#include <string_view>

namespace zylann {
namespace strfuncs_detail {

// Not a big implementation, only what I need.
template <typename T>
std::string_view consume_next_format_placeholder(std::string_view fmt, TextWriter &w, const T &a) {
	const size_t pi = fmt.find("{}");
	if (pi == std::string_view::npos) {
		// Too many arguments supplied?
		w << fmt << " [...]";
		return "";
	}
	w << fmt.substr(0, pi);
	w << a;
	return fmt.substr(pi + 2);
}

template <typename T0>
std::string_view consume_placeholders(std::string_view fmt, TextWriter &w, const T0 &a0) {
	return consume_next_format_placeholder(fmt, w, a0);
}

template <typename T0, typename... TN>
std::string_view consume_placeholders(std::string_view fmt, TextWriter &w, const T0 &a0, const TN &...an) {
	fmt = consume_next_format_placeholder(fmt, w, a0);
	return consume_placeholders(fmt, w, an...);
}

} // namespace strfuncs_detail

template <typename... TN>
StdString format(std::string_view fmt, const TN &...an) {
	StdStringTextWriter w;
	fmt = strfuncs_detail::consume_placeholders(fmt, w, an...);
	w << fmt;
	return w.get_written();
}

#ifdef DEV_ENABLED
StdString to_hex_table(Span<const uint8_t> data);
#endif

} // namespace zylann

#endif // ZN_STRING_FORMAT_H
