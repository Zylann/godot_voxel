#ifndef ZN_GODOT_STD_ALLOCATOR_H
#define ZN_GODOT_STD_ALLOCATOR_H

#include <limits>
// #include <new>

#include "../errors.h"
#include "memory.h"

namespace zylann {

// Default allocator matching standard library requirements.
// When compiling with Godot or GDExtension, it will use Godot's default allocator.
template <class T>
struct StdDefaultAllocator {
	typedef T value_type;

	StdDefaultAllocator() = default;

	template <class U>
	constexpr StdDefaultAllocator(const StdDefaultAllocator<U> &) noexcept {}

	[[nodiscard]] T *allocate(std::size_t n) {
		ZN_ASSERT(n <= std::numeric_limits<std::size_t>::max() / sizeof(T));
		// if (n > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
		// 	throw std::bad_array_new_length();
		// }

		if (T *p = static_cast<T *>(ZN_ALLOC(n * sizeof(T)))) {
			return p;
		}

		// throw std::bad_alloc();
		ZN_CRASH_MSG("Bad alloc");
		return nullptr;
	}

	void deallocate(T *p, std::size_t n) noexcept {
		ZN_FREE(p);
	}

	// Note: defining a `rebind` struct is optional as long as the allocator is a template class. It is therefore
	// provided by `allocator_traits`. `rebind` is used by containers to obtain the same allocator with a different T,
	// in order to allocate internal data structures (nodes of linked list, buckets of unordered_map...)
};

template <class T, class U>
bool operator==(const StdDefaultAllocator<T> &, const StdDefaultAllocator<U> &) {
	return true;
}

template <class T, class U>
bool operator!=(const StdDefaultAllocator<T> &, const StdDefaultAllocator<U> &) {
	return false;
}

} // namespace zylann

#endif // ZN_GODOT_STD_ALLOCATOR_H
