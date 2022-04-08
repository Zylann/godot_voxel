#include <core/os/os.h>

namespace zylann {

bool is_verbose_output_enabled() {
	return OS::get_singleton()->is_stdout_verbose();
}

} // namespace zylann
