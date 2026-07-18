#ifndef ZN_GODOT_STRING_NAMES_H
#define ZN_GODOT_STRING_NAMES_H

#include "core/string_name.h"

namespace zylann::godot {

// StringNames used by ZN_* classes
class StringNames {
private:
	static StringNames *g_singleton;

public:
	static const StringNames &get_singleton();
	static void create_singleton();
	static void destroy_singleton();

	StringNames();

#ifdef TOOLS_ENABLED
	StringName bg;
	StringName Editor;
	StringName font;
	StringName font_color;
	StringName font_size;
	StringName Label;
	StringName Tree;
#endif
};

}; // namespace zylann::godot

#endif
