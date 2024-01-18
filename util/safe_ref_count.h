#ifndef ZN_SAFE_REF_COUNT_H
#define ZN_SAFE_REF_COUNT_H

#include "errors.h"
#include <atomic>

namespace zylann {

// Thread-safe reference counter.
class SafeRefCount {
public:
	SafeRefCount() {}
	SafeRefCount(int initial_count) : _count(initial_count) {}

	inline int add() {
		return _count.fetch_add(1, std::memory_order_acq_rel);
	}

	inline int remove() {
		const int previous_count = _count.fetch_sub(1, std::memory_order_acq_rel);
		ZN_ASSERT_RETURN_V_MSG(
				previous_count != 0, previous_count, "Trying to decrease refcount when it's already zero");
		return previous_count;
	}

	inline int get() const {
		return _count;
	}

private:
	std::atomic_int32_t _count = { 0 };
};

} // namespace zylann

#endif // ZN_SAFE_REF_COUNT_H
