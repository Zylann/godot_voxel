#ifndef ZN_GODOT_CHECK_REF_OWNERSHIP_H
#define ZN_GODOT_CHECK_REF_OWNERSHIP_H

#ifdef TOOLS_ENABLED
#include "../errors.h"
#include "../macros.h"
#include "../string/format.h"
#include "classes/ref_counted.h"
#include "core/string.h"

namespace zylann::godot {

// Checks that nothing takes extra ownership of a RefCounted object between the beginning and the end of a scope.
// This can be used when calling GDVIRTUAL methods that are passed an object that must not be held by the callee after
// the end of the call.
class CheckRefCountDoesNotChange {
public:
	// Note: not taking a `const Ref<RefCounted>&` for convenience, because it may involve casting, which means C++ will
	// not pass Ref<T> by reference but by value instead, which would increase the refcount.
	inline CheckRefCountDoesNotChange(const char *method_name, RefCounted *rc) :
			_method_name(method_name), _rc(rc), _initial_count(rc->get_reference_count()) {}

	inline ~CheckRefCountDoesNotChange() {
		const int after_count = _rc->get_reference_count();
		if (after_count != _initial_count) {
			ZN_PRINT_ERROR(format("Holding a reference to the passed {} outside {} is not allowed (count before: {}, "
								  "count after: {})",
					_rc->get_class(), _method_name, _initial_count, after_count));
		}
	}

private:
	const char *_method_name;
	const RefCounted *_rc;
	const int _initial_count;
};

} // namespace zylann::godot

#define ZN_GODOT_CHECK_REF_COUNT_DOES_NOT_CHANGE(m_ref)                                                                \
	ZN_ASSERT(m_ref.is_valid());                                                                                       \
	zylann::godot::CheckRefCountDoesNotChange ZN_CONCAT(ref_count_checker_, __LINE__)(__FUNCTION__, m_ref.ptr())

#else // TOOLS_ENABLED

#define ZN_GODOT_CHECK_REF_COUNT_DOES_NOT_CHANGE(m_ref)

#endif // TOOLS_ENABLED

#endif // ZN_GODOT_CHECK_REF_OWNERSHIP_H
