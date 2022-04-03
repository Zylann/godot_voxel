#ifndef VOXEL_MACROS_H
#define VOXEL_MACROS_H

#include <core/os/os.h>

// print_verbose() is used everywhere in the engine, but its drawback is that even if you turn it off, strings
// you print are still allocated and formatted, to not be used. This macro avoids the string.
#define PRINT_VERBOSE(msg)                                                                                             \
	if (OS::get_singleton()->is_stdout_verbose()) {                                                                    \
		print_line(msg);                                                                                               \
	}

// TODO Waiting for a fix, Variant() can't be constructed from `size_t` on JavaScript and OSX builds.
// See https://github.com/godotengine/godot/issues/36690
#define SIZE_T_TO_VARIANT(s) static_cast<int64_t>(s)

#define VOXEL_ARRAY_LENGTH(a) (sizeof(a) / sizeof(a[0]))

// Godot does not define the TTR macro for translation of messages in release builds. However, there are some non-editor
// code that can produce errors in this module, and we still want them to compile properly.
#ifdef TOOLS_ENABLED
#define ZN_TTR(msg) TTR(msg)
#else
#define ZN_TTR(msg) msg
#endif

#endif // VOXEL_MACROS_H
