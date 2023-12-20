#ifndef ZN_SHORT_LOCK
#define ZN_SHORT_LOCK

// #define ZN_SHORT_LOCK_IS_MUTEX

#ifdef ZN_SHORT_LOCK_IS_MUTEX
#include "mutex.h"
#else
#include "spin_lock.h"
#endif

namespace zylann {

// A mutex-like primitive that is expected to be locked for short periods of time.
// It can be implemented either with a SpinLock or a Mutex, depending on test results.

#ifdef ZN_SHORT_LOCK_IS_MUTEX
typedef BinaryMutex ShortLock;
#else
typedef SpinLock ShortLock;
#endif

struct ShortLockScope {
	ShortLock &short_lock;
	ShortLockScope(ShortLock &p_sl) : short_lock(p_sl) {
		short_lock.lock();
	}
	~ShortLockScope() {
		short_lock.unlock();
	}
};

} // namespace zylann

#endif // ZN_SHORT_LOCK
