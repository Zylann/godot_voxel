#ifndef ZYLANN_MACROS_H
#define ZYLANN_MACROS_H

// Macros I couldn't put anywhere specific

// TODO Waiting for a fix, Godot's Variant() can't be constructed from `size_t` on JavaScript and OSX builds.
// See https://github.com/godotengine/godot/issues/36690
#define ZN_SIZE_T_TO_VARIANT(s) static_cast<int64_t>(s)

#define ZN_ARRAY_LENGTH(a) (sizeof(a) / sizeof(a[0]))

// Godot does not define the TTR macro for translation of messages in release builds. However, there are some non-editor
// code that can produce errors in this module, and we still want them to compile properly.
#if defined(ZN_GODOT) && defined(TOOLS_ENABLED)

#include <core/string/ustring.h>

#define ZN_TTR(msg) TTR(msg)

#else

#define ZN_TTR(msg) msg

#endif

#endif // ZYLANN_MACROS_H
