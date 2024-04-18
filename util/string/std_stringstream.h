#ifndef ZN_STD_STRINGSTREAM_H
#define ZN_STD_STRINGSTREAM_H

#include "../memory/std_allocator.h"
#include <iosfwd>

namespace zylann {

using StdStringStream = std::basic_stringstream<char, std::char_traits<char>, StdDefaultAllocator<char>>;

} // namespace zylann

#endif // ZN_STD_STRINGSTREAM_H
