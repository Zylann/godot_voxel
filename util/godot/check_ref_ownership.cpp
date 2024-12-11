#include "check_ref_ownership.h"

namespace zylann::godot {

namespace {
bool g_enabled = true;
bool g_reported = false;
} // namespace

void CheckRefCountDoesNotChange::set_enabled(bool enabled) {
	g_enabled = enabled;
}

bool CheckRefCountDoesNotChange::is_enabled() {
	return g_enabled;
}

bool CheckRefCountDoesNotChange::was_reported() {
	return g_reported;
}

void CheckRefCountDoesNotChange::mark_reported() {
	g_reported = true;
}

} // namespace zylann::godot
