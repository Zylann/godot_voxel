#include "../godot/classes/os.h"
#include "../godot/core/print_string.h"
#include "../string/format.h"

#ifdef ZN_DEBUG_LOG_FILE_ENABLED
#include "thread/mutex.h"
#include <fstream>
#endif

namespace zylann {

#ifdef ZN_DEBUG_LOG_FILE_ENABLED

namespace {
Mutex g_log_file_mutex;
bool g_log_to_file = false;
std::ofstream g_log_ofs;
} // namespace

void open_log_file() {
	MutexLock mlock(g_log_file_mutex);
	g_log_to_file = true;
	g_log_ofs.open("zn_log.txt", std::ios::binary | std::ios::trunc);
}

void close_log_file() {
	MutexLock mlock(g_log_file_mutex);
	g_log_to_file = false;
	g_log_ofs.close();
}

void flush_log_file() {
	MutexLock mlock(g_log_file_mutex);
	if (g_log_to_file) {
		g_log_ofs.flush();
	}
}

#endif

bool is_verbose_output_enabled() {
	return OS::get_singleton()->is_stdout_verbose();
}

void print_line(const char *cstr) {
#ifdef ZN_DEBUG_LOG_FILE_ENABLED
	if (g_log_to_file) {
		MutexLock mlock(g_log_file_mutex);
		if (g_log_to_file) {
			g_log_ofs.write(cstr, strlen(cstr));
			g_log_ofs.write("\n", 1);
		}
	}
#else

#if defined(ZN_GODOT)
	::print_line(cstr);
#elif defined(ZN_GODOT_EXTENSION)
	::godot::UtilityFunctions::print(cstr);
#endif

#endif
}

void print_line(const FwdConstStdString &s) {
	print_line(s.s.c_str());
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
	_err_print_error(func, file, line, error.s.c_str());
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
