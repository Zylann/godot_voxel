#ifndef ZN_MUTEX_H
#define ZN_MUTEX_H

#include <mutex>

namespace zylann {

template <class StdMutexT>
class MutexImpl {
	mutable StdMutexT mutex;

public:
	inline void lock() const {
		mutex.lock();
	}

	inline void unlock() const {
		mutex.unlock();
	}

	inline bool try_lock() const {
		return mutex.try_lock();
	}
};

template <class MutexT>
class MutexLock {
	const MutexT &mutex;

public:
	inline explicit MutexLock(const MutexT &p_mutex) : mutex(p_mutex) {
		mutex.lock();
	}

	inline ~MutexLock() {
		mutex.unlock();
	}
};

using Mutex = MutexImpl<std::recursive_mutex>; // Recursive, for general use
using BinaryMutex = MutexImpl<std::mutex>; // Non-recursive, handle with care

// Note: Godot uses a combination of `extern template` and `_ALWAYS_INLINE_` compiler-specific macros instead of
// `inline`. In that setup, without `_ALWAYS_INLINE_`, GCC does not inline methods in debug builds, which then causes
// `undefined reference` errors when linking. However, considering GCC does inline methods in optimized builds, I don't
// understand what we gain from that setup... so I go with simple `inline`.
//
// Don't instantiate these templates in every file where they are used, do it just once
// extern template class MutexImpl<std::recursive_mutex>;
// extern template class MutexImpl<std::mutex>;
// extern template class MutexLock<MutexImpl<std::recursive_mutex>>;
// extern template class MutexLock<MutexImpl<std::mutex>>;

} // namespace zylann

#endif // ZN_MUTEX_H
