#ifndef ZN_GODOT_STRING_H
#define ZN_GODOT_STRING_H

#if defined(ZN_GODOT)
#include <core/string/ustring.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/global_constants.hpp> // For `Error`
#include <godot_cpp/variant/string.hpp>
using namespace godot;
#endif

#include <iosfwd>
#include <string>
#include <string_view>

#ifdef TOOLS_ENABLED
#include "variant.h"
#include <vector>
#endif

#include "../../span.h"

namespace zylann {

inline String to_godot(const std::string_view sv) {
	return String::utf8(sv.data(), sv.size());
}

// Turns out these functions are only used in editor for now.
// They are generic, but I have to wrap them, otherwise GCC throws warnings-as-errors for them being unused.
#ifdef TOOLS_ENABLED

PackedStringArray to_godot(const std::vector<std::string_view> &svv);
PackedStringArray to_godot(const std::vector<std::string> &sv);

#endif

inline std::string to_std_string(const String &godot_string) {
	const CharString cs = godot_string.utf8();
	std::string s = cs.get_data();
	return s;
}

inline Error parse_utf8(String &s, Span<const char> utf8) {
#if defined(ZN_GODOT)
	return s.parse_utf8(utf8.data(), utf8.size());
#elif defined(ZN_GODOT_EXTENSION)
	s.parse_utf8(utf8.data(), utf8.size());
	// The Godot API doesn't return anything, impossible to tell if parsing succeeded.
	return OK;
#endif
}

} // namespace zylann

// `TTR` means "tools translate", which is for editor-only localized messages.
// Godot does not define the TTR macro for translation of messages in release builds. However, there are some non-editor
// code that can produce errors in this module, and we still want them to compile properly.
// TODO GDX: `TTR` is missing from `GodotCpp`.
#if defined(ZN_GODOT) && defined(TOOLS_ENABLED)
#define ZN_TTR(msg) TTR(msg)
#else
#define ZN_TTR(msg) String(msg)
#endif

ZN_GODOT_NAMESPACE_BEGIN

// Needed for `zylann::format()`.
// I gave up trying to nicely convert Godot's String here... it has non-explicit `const char*` constructor, that makes
// other overloads ambiguous...
// std::stringstream &operator<<(std::stringstream &ss, const String &s);
struct GodotStringWrapper {
	GodotStringWrapper(const String &p_s) : s(p_s) {}
	const String &s;
};
std::stringstream &operator<<(std::stringstream &ss, GodotStringWrapper s);

ZN_GODOT_NAMESPACE_END

namespace std {

// For String keys in std::unordered_map
template <>
struct hash<String> {
	inline size_t operator()(const String &v) const {
		return v.hash();
	}
};

} // namespace std

#endif // ZN_GODOT_STRING_H
