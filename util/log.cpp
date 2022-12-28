#include "godot/funcs.h"
#include "string_funcs.h"

#include "godot/classes/os.h"
#include "godot/core/print_string.h"

namespace zylann {

bool is_verbose_output_enabled() {
	return OS::get_singleton()->is_stdout_verbose();
}

void println(const char *cstr) {
#if defined(ZN_GODOT)
	print_line(cstr);
#elif defined(ZN_GODOT_EXTENSION)
	godot::UtilityFunctions::print(cstr);
#endif
}

void println(const FwdConstStdString &s) {
	println(s.s.c_str());
}

void print_warning(const char *warning, const char *func, const char *file, int line) {
#if defined(ZN_GODOT)
	_err_print_error(func, file, line, warning, false, ERR_HANDLER_WARNING);
#elif defined(ZN_GODOT_EXTENSION)
	_err_print_error(func, file, line, warning, true);
#endif
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
#if defined(ZN_GODOT)
	_err_flush_stdout();
#endif
	// TODO GodotCpp does not expose a way to flush logging output. Crashing errors might then miss log messages.
}

} // namespace zylann
