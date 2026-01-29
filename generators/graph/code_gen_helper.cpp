#include "code_gen_helper.h"
#include "../../util/containers/fixed_array.h"
#include "../../util/errors.h"
#include "../../util/string/format.h"

#include <cstring>
#include <sstream>

namespace zylann {

void CodeGenHelper::indent() {
	++_indent_level;
}

void CodeGenHelper::dedent() {
	ZN_ASSERT(_indent_level > 0);
	--_indent_level;
}

void CodeGenHelper::add(const char *s, unsigned int len) {
	for (unsigned int i = 0; i < len; ++i) {
		const char c = s[i];
		if (_newline) {
			for (unsigned int j = 0; j < _indent_level; ++j) {
				_main_code += "    ";
			}
			_newline = false;
		}
		_main_code += c;
		if (c == '\n') {
			_newline = true;
		}
	}
}

void CodeGenHelper::add(const char *s) {
	add(s, strlen(s));
}

void CodeGenHelper::add(const StdString &s) {
	add(s.c_str(), s.size());
}

void CodeGenHelper::add(float x) {
	FixedArray<char, 32> buffer;
	// Godot shaders want float constants to be explicit
	const unsigned int decimals = float(int(x)) == x ? 1 : 10;
	const unsigned int len = snprintf(buffer.data(), buffer.size(), "%.*f", decimals, x);
	add(buffer.data(), len);
}

void CodeGenHelper::add(double x) {
	FixedArray<char, 32> buffer;
	// Godot shaders want float constants to be explicit
	const unsigned int decimals = double(int(x)) == x ? 1 : 16;
	const unsigned int len = snprintf(buffer.data(), buffer.size(), "%.*lf", decimals, x);
	add(buffer.data(), len);
}

void CodeGenHelper::add(int x) {
	FixedArray<char, 32> buffer;
	const unsigned int len = snprintf(buffer.data(), buffer.size(), "%i", x);
	add(buffer.data(), len);
}

void CodeGenHelper::require_lib_code(const char *lib_name, const char *code) {
	auto p = _included_libs.insert(lib_name);
	if (p.second) {
		_lib_code += "\n\n";
		_lib_code += code;
		_lib_code += "\n\n";
	}
}

// Some code can be too big to fit in a single literal depending on the compiler,
// so an option is to provide it as a zero-terminated array of string literals
void CodeGenHelper::require_lib_code(const char *lib_name, const char **code) {
	auto p = _included_libs.insert(lib_name);
	if (p.second) {
		_lib_code += "\n\n";
		while (*code != 0) {
			_lib_code += *code;
			++code;
		}
		_lib_code += "\n\n";
	}
}

StdString CodeGenHelper::generate_var_name() {
	const StdString s = format("v{}", _next_var_name_id);
	++_next_var_name_id;
	return s;
}

StdString CodeGenHelper::print() const {
	StdString out = _lib_code;
	out += _main_code;
	return out;
}

} // namespace zylann
