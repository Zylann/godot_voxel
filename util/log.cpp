#include "godot/funcs.h"
#include "string_funcs.h"
#include <core/os/os.h>

namespace zylann {

bool is_verbose_output_enabled() {
	return OS::get_singleton()->is_stdout_verbose();
}

void println(const char *cstr) {
	print_line(cstr);
}

void println(const FwdConstStdString &s) {
	print_line(s.s.c_str());
}

void print_warning(const char *warning, const char *func, const char *file, int line) {
	_err_print_error(func, file, line, warning, false, ERR_HANDLER_WARNING);
}

void print_warning(const FwdConstStdString &warning, const char *func, const char *file, int line) {
	print_warning(warning.s.c_str(), func, file, line);
}

void print_error(FwdConstStdString error, const char *func, const char *file, int line) {
	print_error(error.s.c_str(), func, file, line);
}

void print_error(const char *error, const char *func, const char *file, int line) {
	_err_print_error(func, file, line, error);
}

void print_error(const char *error, const char *msg, const char *func, const char *file, int line) {
	_err_print_error(func, file, line, error, msg);
}

void print_error(const char *error, const FwdConstStdString &msg, const char *func, const char *file, int line) {
	_err_print_error(func, file, line, error, msg.s.c_str());
}

void flush_stdout() {
	_err_flush_stdout();
}

} // namespace zylann
