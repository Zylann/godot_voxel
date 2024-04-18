#ifndef ZYLANN_LOG_H
#define ZYLANN_LOG_H

#include "../string/fwd_std_string.h"

// print_verbose() is used everywhere in Godot, but its drawback is that even if you turn it off, strings
// you print are still allocated and formatted, to not be used. This macro avoids the string.
#define ZN_PRINT_VERBOSE(msg)                                                                                          \
	if (zylann::is_verbose_output_enabled()) {                                                                         \
		zylann::print_line(msg);                                                                                       \
	}

#define ZN_PRINT_WARNING(msg) zylann::print_warning(msg, __FUNCTION__, __FILE__, __LINE__)
#define ZN_PRINT_ERROR(msg) zylann::print_error(msg, __FUNCTION__, __FILE__, __LINE__)

namespace zylann {

bool is_verbose_output_enabled();

void print_line(const char *cstr);
void print_line(const FwdConstStdString &s);

void print_warning(const char *warning, const char *func, const char *file, int line);
void print_warning(const FwdConstStdString &warning, const char *func, const char *file, int line);

void print_error(FwdConstStdString error, const char *func, const char *file, int line);
void print_error(const char *error, const char *func, const char *file, int line);
void print_error(const char *error, const char *msg, const char *func, const char *file, int line);
void print_error(const char *error, const FwdConstStdString &msg, const char *func, const char *file, int line);

void flush_stdout();

// When defined, redirects `println` to a file instead of standard output.
// #define ZN_DEBUG_LOG_FILE_ENABLED

#ifdef ZN_DEBUG_LOG_FILE_ENABLED

void open_log_file();
void close_log_file();
void flush_log_file();

#endif

} // namespace zylann

#endif // ZYLANN_LOG_H
