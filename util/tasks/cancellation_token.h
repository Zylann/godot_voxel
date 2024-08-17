#ifndef ZN_CANCELLATION_TOKEN_H
#define ZN_CANCELLATION_TOKEN_H

#include "../errors.h"
#include "../memory/memory.h"
#include <atomic>

namespace zylann {

// Simple object shared between a task and the requester of the task. Allows the requester to cancel the task before it
// runs or finishes.
class TaskCancellationToken {
public:
	// TODO Could be optimized
	// - Pointer to an atomic refcount?
	// - Index into a [paged] pool of atomic ints?

	static TaskCancellationToken create() {
		TaskCancellationToken token;
		token._cancelled = make_shared_instance<std::atomic_bool>(false);
		return token;
	}

	inline bool is_valid() const {
		return _cancelled != nullptr;
	}

	inline void cancel() {
#ifdef TOOLS_ENABLED
		ZN_ASSERT(_cancelled != nullptr);
#endif
		*_cancelled = true;
	}

	inline bool is_cancelled() const {
#ifdef TOOLS_ENABLED
		ZN_ASSERT(_cancelled != nullptr);
#endif
		return *_cancelled;
	}

private:
	std::shared_ptr<std::atomic_bool> _cancelled;
};

} // namespace zylann

#endif // ZN_CANCELLATION_TOKEN_H
