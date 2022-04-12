#include <sstream>
#include <string>
#include <string_view>

namespace zylann {

struct FwdConstStdString {
	const std::string &s;
	FwdConstStdString(const std::string &p_s) : s(p_s) {}
};

struct FwdMutableStdString {
	std::string &s;
	FwdMutableStdString(std::string &p_s) : s(p_s) {}
};

namespace strfuncs_detail {

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

template <typename T0>
std::string_view consume_placeholders(std::string_view fmt, std::stringstream &ss, const T0 &a0) {
	return consume_next_format_placeholder(fmt, ss, a0);
}

template <typename T0, typename... TN>
std::string_view consume_placeholders(std::string_view fmt, std::stringstream &ss, const T0 &a0, const TN &...an) {
	fmt = consume_next_format_placeholder(fmt, ss, a0);
	return consume_placeholders(fmt, ss, an...);
}

} // namespace strfuncs_detail

template <typename... TN>
std::string format(std::string_view fmt, const TN &...an) {
	std::stringstream ss;
	fmt = strfuncs_detail::consume_placeholders(fmt, ss, an...);
	ss << fmt;
	return ss.str();
}

} // namespace zylann
