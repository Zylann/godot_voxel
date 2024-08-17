#ifndef ZN_STD_VECTOR_H
#define ZN_STD_VECTOR_H

#include "../memory/std_allocator.h"
#include <vector>

namespace zylann {

// Convenience alias that uses our own default allocator. When using Godot, it will use Godot's default allocator.
// (in contrast, direct std::vector always uses the standard library's default allocator)
template <typename TValue, typename TAllocator = StdDefaultAllocator<TValue>>
using StdVector = std::vector<TValue, TAllocator>;

} // namespace zylann

#endif // ZN_STD_VECTOR_H
