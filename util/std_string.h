#ifndef ZN_STD_STRING_H
#define ZN_STD_STRING_H

#include "memory/std_allocator.h"
#include <string>

namespace zylann {

using StdString = std::basic_string<char, std::char_traits<char>, StdDefaultAllocator<char>>;

} // namespace zylann

#endif // ZN_STD_STRING_H
