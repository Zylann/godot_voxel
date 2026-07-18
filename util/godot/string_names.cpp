#include "string_names.h"
#include "../errors.h"
#include "../memory/memory.h"

namespace zylann::godot {

StringNames *StringNames::g_singleton;

void StringNames::create_singleton() {
	ZN_ASSERT(g_singleton == nullptr);
	g_singleton = ZN_NEW(StringNames);
}

void StringNames::destroy_singleton() {
	ZN_ASSERT(g_singleton != nullptr);
	ZN_DELETE(g_singleton);
	g_singleton = nullptr;
}

const StringNames &StringNames::get_singleton() {
	ZN_ASSERT(g_singleton != nullptr);
	return *g_singleton;
}

StringNames::StringNames() {
#ifdef TOOLS_ENABLED
	bg = StringName("bg");
	Editor = StringName("editor");
	font = StringName("font");
	font_color = StringName("font_color");
	font_size = StringName("font_size");
	Label = StringName("label");
	Tree = StringName("Tree");
#endif
}

} // namespace zylann::godot
