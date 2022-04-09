#include <core/os/os.h>

namespace zylann {

bool is_verbose_output_enabled() {
	return OS::get_singleton()->is_stdout_verbose();
}

void println(const char *cstr) {
	print_line(cstr);
}

void println(const std::string &s) {
	print_line(s.c_str());
}

} // namespace zylann
