// This file is GDExtension-only, tools-only.

#include "editor_scale.h"
#include "../errors.h"
#include <godot_cpp/classes/editor_interface.hpp>

namespace zylann {

float get_editor_scale() {
	const ::godot::EditorInterface *ei = ::godot::EditorInterface::get_singleton();
	ZN_ASSERT_RETURN_V(ei != nullptr, 1.f);
	return ei->get_editor_scale();
}

} // namespace zylann
