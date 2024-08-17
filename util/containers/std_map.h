#ifndef ZN_STD_MAP_H
#define ZN_STD_MAP_H

#include "../memory/std_allocator.h"
#include <map>

namespace zylann {

// Convenience alias that uses our own default allocator
template < //
		typename TKey, //
		typename TValue, //
		typename TLess = std::less<TKey>, //
		typename TAllocator = StdDefaultAllocator<std::pair<const TKey, TValue>> //
		>
using StdMap = std::map<TKey, TValue, TLess, TAllocator>;

} // namespace zylann

#endif // ZN_STD_MAP_H
