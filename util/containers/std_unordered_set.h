#ifndef ZN_STD_UNORDERED_SET_H
#define ZN_STD_UNORDERED_SET_H

#include "../memory/std_allocator.h"
#include <unordered_set>

namespace zylann {

// Convenience alias that uses our own default allocator
template < //
		typename TValue, //
		typename THasher = std::hash<TValue>, //
		typename TEquator = std::equal_to<TValue>, //
		typename TAllocator = StdDefaultAllocator<TValue> //
		>
using StdUnorderedSet = std::unordered_set<TValue, THasher, TEquator, TAllocator>;

} // namespace zylann

#endif // ZN_STD_UNORDERED_SET_H
