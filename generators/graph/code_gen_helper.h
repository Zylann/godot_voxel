#ifndef ZN_CODE_GEN_HELPER_H
#define ZN_CODE_GEN_HELPER_H

#include "../../util/containers/std_unordered_set.h"
#include "../../util/string/std_string.h"
// Stopped using StdStringstream for the time being, because of issues supporting some Linux distros
// https://github.com/Zylann/godot_voxel/issues/842
// #include "../../util/string/std_stringstream.h"

namespace zylann {

class CodeGenHelper {
public:
	void indent();
	void dedent();

	void add(const char *s, unsigned int len);
	void add(const char *s);
	void add(const StdString &s);
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
	StdString generate_var_name();

	StdString print() const;

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

	StdString _main_code;
	StdString _lib_code;
	unsigned int _indent_level = 0;
	unsigned int _next_var_name_id = 0;
	StdUnorderedSet<const char *> _included_libs;
	bool _newline = true;
};

} // namespace zylann

#endif // ZN_CODE_GEN_HELPER_H
