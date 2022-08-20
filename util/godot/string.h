#ifndef ZN_GODOT_STRING_H
#define ZN_GODOT_STRING_H

#include <core/string/ustring.h>
#include <iosfwd>
#include <string_view>

#ifdef TOOLS_ENABLED
#include <core/variant/variant.h>
#include <vector>
#endif

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

} // namespace zylann

// Needed for `zylann::format()`.
// I gave up trying to nicely convert Godot's String here... it has non-explicit `const char*` constructor, that makes
// other overloads ambiguous...
//std::stringstream &operator<<(std::stringstream &ss, const String &s);
struct GodotStringWrapper {
	GodotStringWrapper(const String &p_s) : s(p_s) {}
	const String &s;
};
std::stringstream &operator<<(std::stringstream &ss, GodotStringWrapper s);

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
