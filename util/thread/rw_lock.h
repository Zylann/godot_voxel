#ifndef ZN_RW_LOCK_H
#define ZN_RW_LOCK_H

#include <shared_mutex>

//#define ZN_PROFILE_RWLOCK
#ifdef ZN_PROFILE_RWLOCK
#include "../profiling.h"
#endif

namespace zylann {

class RWLock {
public:
	// Lock the rwlock, block if locked by someone else
	void read_lock() const {
#ifdef ZN_PROFILE_RWLOCK
		ZN_PROFILE_SCOPE();
#endif
		_mutex.lock_shared();
	}

	// Unlock the rwlock, let other threads continue
	void read_unlock() const {
		_mutex.unlock_shared();
	}

	// Attempt to lock the rwlock, returns `true` on success, `false` means it can't lock.
	bool read_try_lock() const {
		return _mutex.try_lock_shared();
	}

	// Lock the rwlock, block if locked by someone else
	void write_lock() {
#ifdef ZN_PROFILE_RWLOCK
		ZN_PROFILE_SCOPE();
#endif
		_mutex.lock();
	}

	// Unlock the rwlock, let other thwrites continue
	void write_unlock() {
		_mutex.unlock();
	}

	// Attempt to lock the rwlock, returns `true` on success, `false` means it can't lock.
	bool write_try_lock() {
		return _mutex.try_lock();
	}

private:
	mutable std::shared_timed_mutex _mutex;
};

class RWLockRead {
public:
	RWLockRead(const RWLock &p_lock) : _lock(p_lock) {
		_lock.read_lock();
	}
	~RWLockRead() {
		_lock.read_unlock();
	}

private:
	const RWLock &_lock;
};

class RWLockWrite {
public:
	RWLockWrite(RWLock &p_lock) : _lock(p_lock) {
		_lock.write_lock();
	}
	~RWLockWrite() {
		_lock.write_unlock();
	}

private:
	RWLock &_lock;
};

} // namespace zylann

#endif // ZN_RW_LOCK_H
