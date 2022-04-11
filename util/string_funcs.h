#include <sstream>
#include <string>
#include <string_view>

namespace zylann {

struct FwdStdString {
	const std::string &s;
	FwdStdString(const std::string &p_s) : s(p_s) {}
};

// Not a big implementation, only what I need.
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
