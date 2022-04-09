#ifndef ZYLANN_LOG_H
#define ZYLANN_LOG_H

#include <sstream>
#include <string>
#include <string_view>

// print_verbose() is used everywhere in Godot, but its drawback is that even if you turn it off, strings
// you print are still allocated and formatted, to not be used. This macro avoids the string.
#define ZN_PRINT_VERBOSE(msg)                                                                                          \
	if (zylann::is_verbose_output_enabled()) {                                                                         \
		zylann::println(msg);                                                                                          \
	}

namespace zylann {

bool is_verbose_output_enabled();

// TODO Can't use `print_line` because Godot defines it as a macro
void println(const char *cstr);
void println(const std::string &s);

template <typename T>
std::string_view consume_next_format_placeholder(std::string_view fmt, std::stringstream &ss, const T &a) {
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

template <typename T>
std::string format(std::string_view fmt, T a) {
	std::stringstream ss;
	fmt = consume_next_format_placeholder(fmt, ss, a);
	ss << fmt;
	return ss.str();
}

template <typename T0, typename T1>
std::string format(std::string_view fmt, T0 a0, T1 a1) {
	std::stringstream ss;
	fmt = consume_next_format_placeholder(fmt, ss, a0);
	fmt = consume_next_format_placeholder(fmt, ss, a1);
	ss << fmt;
	return ss.str();
}

template <typename T0, typename T1, typename T2>
std::string format(std::string_view fmt, T0 a0, T1 a1, T2 a2) {
	std::stringstream ss;
	fmt = consume_next_format_placeholder(fmt, ss, a0);
	fmt = consume_next_format_placeholder(fmt, ss, a1);
	fmt = consume_next_format_placeholder(fmt, ss, a2);
	ss << fmt;
	return ss.str();
}

template <typename T0, typename T1, typename T2, typename T3>
std::string format(std::string_view fmt, T0 a0, T1 a1, T2 a2, T3 a3) {
	std::stringstream ss;
	fmt = consume_next_format_placeholder(fmt, ss, a0);
	fmt = consume_next_format_placeholder(fmt, ss, a1);
	fmt = consume_next_format_placeholder(fmt, ss, a2);
	fmt = consume_next_format_placeholder(fmt, ss, a3);
	ss << fmt;
	return ss.str();
}

} // namespace zylann

#endif // ZYLANN_LOG_H
