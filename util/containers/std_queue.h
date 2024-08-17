#ifndef ZN_STD_QUEUE_H
#define ZN_STD_QUEUE_H

#include "../memory/std_allocator.h"
#include <deque>
#include <queue>

namespace zylann {

// Convenience alias that uses our own default allocator
template <typename TValue, typename TAllocator = StdDefaultAllocator<TValue>>
using StdQueue = std::queue<TValue, std::deque<TValue, TAllocator>>;

} // namespace zylann

#endif // ZN_STD_QUEUE_H
