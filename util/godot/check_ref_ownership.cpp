#include "check_ref_ownership.h"

namespace zylann::godot {

namespace {
bool g_enabled = true;
}

void CheckRefCountDoesNotChange::set_enabled(bool enabled) {
	g_enabled = enabled;
}

bool CheckRefCountDoesNotChange::is_enabled() {
	return g_enabled;
}

} // namespace zylann::godot
