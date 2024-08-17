#include "std_allocator.h"

namespace zylann {

#ifdef DEBUG_ENABLED
namespace StdDefaultAllocatorCounters {
std::atomic_uint64_t g_allocated;
std::atomic_uint64_t g_deallocated;
} // namespace StdDefaultAllocatorCounters
#endif

} // namespace zylann
