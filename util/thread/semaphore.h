#ifndef ZN_SEMAPHORE_H
#define ZN_SEMAPHORE_H

#include <condition_variable>
#include <mutex>

namespace zylann {

class Semaphore {
public:
	inline void post() const {
		std::lock_guard<decltype(_mutex)> lock(_mutex);
		++_count;
		_condition.notify_one();
	}

	inline void wait() const {
		std::unique_lock<decltype(_mutex)> lock(_mutex);
		while (_count == 0) { // Handle spurious wake-ups.
			_condition.wait(lock);
		}
		--_count;
	}

	inline bool try_wait() const {
		std::lock_guard<decltype(_mutex)> lock(_mutex);
		if (_count != 0) {
			--_count;
			return true;
		}
		return false;
	}

private:
	mutable std::mutex _mutex;
	mutable std::condition_variable _condition;
	mutable unsigned long _count = 0; // Initialized as locked.
};

} // namespace zylann

#endif // ZN_SEMAPHORE_H
