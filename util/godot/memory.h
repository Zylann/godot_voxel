#ifndef ZN_GODOT_MEMORY_H
#define ZN_GODOT_MEMORY_H

#if defined(ZN_GODOT)
#include <core/os/memory.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/core/memory.hpp>
#endif

#include <memory>

namespace zylann::godot {

/*// Creates a shared_ptr which will always use Godot's allocation functions
template <typename T>
inline std::shared_ptr<T> gd_make_shared() {
	// std::make_shared() apparently wont allow us to specify custom new and delete
	return std::shared_ptr<T>(memnew(T), memdelete<T>);
}*/

// For use with smart pointers such as std::unique_ptr
template <typename T>
struct ObjectDeleter {
	inline void operator()(T *obj) {
		memdelete(obj);
	}
};

// Specialization of `std::unique_ptr which always uses Godot's `memdelete()` as deleter.
template <typename T>
using ObjectUniquePtr = std::unique_ptr<T, ObjectDeleter<T>>;

// Creates a `GodotObjectUniquePtr<T>` with an object constructed with `memnew()` inside.
template <typename T>
ObjectUniquePtr<T> make_unique() {
	return ObjectUniquePtr<T>(memnew(T));
}

} // namespace zylann::godot

#endif // ZN_GODOT_MEMORY_H
