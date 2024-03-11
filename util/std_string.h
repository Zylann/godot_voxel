#ifndef ZN_STD_STRING_H
#define ZN_STD_STRING_H

#include "memory/std_allocator.h"
#include <string>

#ifdef __GNUC__
#include <string_view>
#endif

namespace zylann {

using StdString = std::basic_string<char, std::char_traits<char>, StdDefaultAllocator<char>>;

} // namespace zylann

#ifdef __GNUC__

// Attempt at fixing GCC having trouble dealing with `unordered_map<StdString, V> map;`.
// I couldn't understand why exactly that happens, whether it's a bug or not. In Compiler Explorer, all versions prior
// to GCC 13.1 fail to compile such code, except from 13.1 onwards. Manually defining a hash specialization for our
// alias seems to workaround it.
namespace std {
template <>
struct hash<zylann::StdString> {
	size_t operator()(const zylann::StdString &v) const {
		const std::string_view s(v);
		std::hash<std::string_view> hasher;
		return hasher(s);
	}
};
} // namespace std

#endif

#endif // ZN_STD_STRING_H
