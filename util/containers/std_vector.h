#ifndef ZN_STD_VECTOR_H
#define ZN_STD_VECTOR_H

#include "../memory/std_allocator.h"
#include "span.h"
#include <vector>

namespace zylann {

// Convenience alias that uses our own default allocator. When using Godot, it will use Godot's default allocator.
// (in contrast, direct std::vector always uses the standard library's default allocator)
template <typename TValue, typename TAllocator = StdDefaultAllocator<TValue>>
using StdVector = std::vector<TValue, TAllocator>;

template <typename TValue, typename TAllocator>
Span<TValue> to_span(std::vector<TValue, TAllocator> &vec) {
	return Span<TValue>(vec.data(), 0, vec.size());
}

template <typename TValue, typename TAllocator>
Span<const TValue> to_span(const std::vector<TValue, TAllocator> &vec) {
	return Span<const TValue>(vec.data(), 0, vec.size());
}

template <typename TValue, typename TAllocator>
Span<TValue> to_span_from_position_and_size(std::vector<TValue, TAllocator> &vec, unsigned int pos, unsigned int size) {
	ZN_ASSERT(pos + size <= vec.size());
	return Span<TValue>(vec.data(), pos, pos + size);
}

template <typename TValue, typename TAllocator>
Span<const TValue> to_span_from_position_and_size(
		const std::vector<TValue, TAllocator> &vec,
		unsigned int pos,
		unsigned int size
) {
	ZN_ASSERT(pos + size <= vec.size());
	return Span<const TValue>(vec.data(), pos, pos + size);
}

// TODO Deprecate, now Span has a conversion constructor that can allow doing that
template <typename TValue, typename TAllocator>
Span<const TValue> to_span_const(const std::vector<TValue, TAllocator> &vec) {
	return Span<const TValue>(vec.data(), 0, vec.size());
}

} // namespace zylann

#endif // ZN_STD_VECTOR_H
