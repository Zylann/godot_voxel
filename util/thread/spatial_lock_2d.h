#ifndef ZN_SPATIAL_LOCK_2D_H
#define ZN_SPATIAL_LOCK_2D_H

#include "../containers/std_vector.h"
#include "../math/box_bounds_2i.h"
#include "mutex.h"
#include "semaphore.h"
#include "short_lock.h"
#include "thread.h"

#ifdef TOOLS_ENABLED
#define ZN_SPATIAL_LOCK_2D_CHECKS
#endif

namespace zylann::voxel {

// TODO Use template? This is a litteral copy paste from the 3D version with 2 instead of 3.

// Locking on a large 2D data structure can be done with this, instead of putting RWLocks on every chunk or
// every node. This also reduces the amount of required mutexes considerably (that matters on some platforms with
// low limits).
//
// Some methods allow to `try` locking areas. They should be used in contextes where blocking is worse than
// delaying operations. Such contextes may then postpone their work, or cancel it. If that's not possible, then it
// has to wait, or run outside of the main thread to maintain app responsivity.
//
// Do not try to lock more than one box at the same time before doing your task. If another thread does so,
// it could end up in a deadlock depending in the order it happens.
class SpatialLock2D {
public:
	enum Mode { //
		MODE_READ = 0,
		MODE_WRITE = 1
	};

	struct Box {
		BoxBounds2i bounds;
		Mode mode;
#ifdef ZN_SPATIAL_LOCK_2D_CHECKS
		Thread::ID thread_id;
#endif
	};

	SpatialLock2D();

	~SpatialLock2D() {
		ZN_ASSERT_RETURN(_boxes.size() == 0);
	}

	bool try_lock_read(const BoxBounds2i &box) {
		_boxes_mutex.lock();
		if (can_lock_for_read(box)) {
			_boxes.push_back(Box{ box, MODE_READ,
#ifdef ZN_SPATIAL_LOCK_2D_CHECKS
					Thread::get_caller_id()
#endif
			});
			_boxes_mutex.unlock();
			return true;
		} else {
			_boxes_mutex.unlock();
			return false;
		}
	}

	void lock_read(const BoxBounds2i &box) {
		while (try_lock_read(box) == false) {
			_semaphore.wait();
		}
	}

	inline void unlock_read(const BoxBounds2i &box) {
		unlock(box, MODE_READ);
	}

	bool try_lock_write(const BoxBounds2i &box) {
		_boxes_mutex.lock();
		if (can_lock_for_write(box)) {
			_boxes.push_back(Box{ box, MODE_WRITE,
#ifdef ZN_SPATIAL_LOCK_2D_CHECKS
					Thread::get_caller_id()
#endif
			});
			_boxes_mutex.unlock();
			return true;
		} else {
			_boxes_mutex.unlock();
			return false;
		}
	}

	void lock_write(const BoxBounds2i &box) {
		while (try_lock_write(box) == false) {
			_semaphore.wait();
		}
	}

	inline void unlock_write(const BoxBounds2i &box) {
		unlock(box, MODE_WRITE);
	}

	inline int get_locked_boxes_count() const {
		ShortLockScope rlock(_boxes_mutex);
		return _boxes.size();
	}

	// Scoped helpers

	struct Read {
		Read(SpatialLock2D &p_locker, const BoxBounds2i p_box) : locker(p_locker), box(p_box) {
			locker.lock_read(box);
		}
		~Read() {
			locker.unlock_read(box);
		}
		SpatialLock2D &locker;
		const BoxBounds2i box;
	};

	struct Write {
		Write(SpatialLock2D &p_locker, const BoxBounds2i p_box) : locker(p_locker), box(p_box) {
			locker.lock_write(box);
		}
		~Write() {
			locker.unlock_write(box);
		}
		SpatialLock2D &locker;
		const BoxBounds2i box;
	};

	struct UnlockWriteOnScopeExit {
		UnlockWriteOnScopeExit(SpatialLock2D &p_locker, const BoxBounds2i p_box) : locker(p_locker), box(p_box) {}
		~UnlockWriteOnScopeExit() {
			locker.unlock_write(box);
		}
		SpatialLock2D &locker;
		const BoxBounds2i box;
	};

	struct UnlockReadOnScopeExit {
		UnlockReadOnScopeExit(SpatialLock2D &p_locker, const BoxBounds2i p_box) : locker(p_locker), box(p_box) {}
		~UnlockReadOnScopeExit() {
			locker.unlock_read(box);
		}
		SpatialLock2D &locker;
		const BoxBounds2i box;
	};

private:
	bool can_lock_for_read(const BoxBounds2i &box) {
#ifdef ZN_SPATIAL_LOCK_2D_CHECKS
		const Thread::ID thread_id = Thread::get_caller_id();
#endif

		for (unsigned int i = 0; i < _boxes.size(); ++i) {
			const Box &existing_box = _boxes[i];
#ifdef ZN_SPATIAL_LOCK_2D_CHECKS
			// Each thread can lock only one box at a time, otherwise there can be deadlocks depending on the order of
			// locks. For example:
			// - Thread 1 locks A
			// - Thread 2 locks B
			// - Thread 1 locks B, but blocks because it is already locked
			// - Thread 2 locks A, but blocks because it is already locked:
			//   This is a deadlock.
			// Note: this is not true if threads only lock for reading, but if we didn't ever write we'd not use locks.
			// Note: this is also not true if threads use `try_lock` instead!
			ZN_ASSERT_RETURN_V_MSG(existing_box.thread_id != thread_id, false,
					"Locking two areas from the same thread is not allowed");
#endif
			if (existing_box.bounds.intersects(box) && existing_box.mode == MODE_WRITE) {
				return false;
				break;
			}
		}
		return true;
	}

	bool can_lock_for_write(const BoxBounds2i &box) {
#ifdef ZN_SPATIAL_LOCK_2D_CHECKS
		const Thread::ID thread_id = Thread::get_caller_id();
#endif

		for (unsigned int i = 0; i < _boxes.size(); ++i) {
			const Box &existing_box = _boxes[i];

#ifdef ZN_SPATIAL_LOCK_2D_CHECKS
			ZN_ASSERT_RETURN_V_MSG(existing_box.thread_id != thread_id, false,
					"Locking two areas from the same thread is not allowed");
#endif
			if (existing_box.bounds.intersects(box)) {
				return false;
				break;
			}
		}
		return true;
	}

	void remove_box(const BoxBounds2i &box, Mode mode);

	void unlock(const BoxBounds2i &box, Mode mode) {
		_boxes_mutex.lock();
		remove_box(box, mode);
		_boxes_mutex.unlock();
		// Tell eventual waiting threads that they might be able to lock their box now.
		_semaphore.post();
	}

	// List of boxes currently locked.
	// In practice, each thread can lock up to 1 box at once (maybe a few more in rare cases that would allow it), so
	// there won't be many boxes to store.
	StdVector<Box> _boxes;
	// This mutex is supposed to be locked for very small periods of time, just to lookup, add or remove boxes.
	// So we lock it even in `try_*` methods. The long-period locking states are the boxes themselves.
	// Also it is not a recursive mutex for performance. Do not lock it again once you successfully locked it.
	mutable ShortLock _boxes_mutex;
	// This semaphore is waited for when a lock fails. It is posted everytime a box is unlocked, so any thread
	// waiting for it may retry locking their box.
	Semaphore _semaphore;
};

} // namespace zylann::voxel

#endif // ZN_SPATIAL_LOCK_2D_H
