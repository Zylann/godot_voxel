#ifndef ZN_CODE_GEN_HELPER_H
#define ZN_CODE_GEN_HELPER_H

#include "../../util/fwd_std_string.h"
#include <iosfwd>
#include <unordered_set>

namespace zylann {

class CodeGenHelper {
public:
	CodeGenHelper(std::stringstream &main_ss, std::stringstream &lib_ss);

	void indent();
	void dedent();

	void add(const char *s, unsigned int len);
	void add(const char *s);
	void add(FwdConstStdString s);
	void add(double x);
	void add(float x);
	void add(int x);

	template <typename T>
	void add_format(const char *fmt, const T &a0) {
		fmt = _add_format(fmt, a0);
		if (*fmt != '\0') {
			add(fmt);
		}
	}

	template <typename T0, typename... TN>
	void add_format(const char *fmt, const T0 &a0, const TN &...an) {
		fmt = _add_format(fmt, a0);
		add_format(fmt, an...);
	}

	void require_lib_code(const char *lib_name, const char *code);
	void require_lib_code(const char *lib_name, const char **code);
	void generate_var_name(FwdMutableStdString out_var_name);

	void print(FwdMutableStdString output);

private:
	template <typename T>
	const char *_add_format(const char *fmt, const T &a0) {
		if (*fmt == '\0') {
			return fmt;
		}
		const char *c = fmt;
		while (*c != '\0') {
			if (*c == '{') {
				++c;
				if (*c == '}') {
					add(fmt, c - fmt - 1);
					add(a0);
					return c + 1;
				}
			}
			++c;
		}
		add(fmt, c - fmt);
		return c;
	}

	std::stringstream &_main_ss;
	std::stringstream &_lib_ss;
	unsigned int _indent_level = 0;
	unsigned int _next_var_name_id = 0;
	std::unordered_set<const char *> _included_libs;
	bool _newline = true;
};

} // namespace zylann

#endif // ZN_CODE_GEN_HELPER_H
