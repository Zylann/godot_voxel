#ifndef ZYLANN_MEMORY_H
#define ZYLANN_MEMORY_H

#include <memory>

// Default new and delete operators.
#if defined(ZN_GODOT)

#include <core/os/memory.h>

// Use Godot's allocator.
// In modules, memnew and memdelete work for anything. However in GDExtension it might not be the case...
#define ZN_NEW(t) memnew(t)
#define ZN_DELETE(t) memdelete(t)
#define ZN_ALLOC(size) memalloc(size)
#define ZN_REALLOC(p, size) memrealloc(p, size)
#define ZN_FREE(p) memfree(p)

#elif defined(ZN_GODOT_EXTENSION)

#include <godot_cpp/core/memory.hpp>

#define ZN_NEW(t) memnew(t)
#define ZN_DELETE(t) ::godot::memdelete(t)
#define ZN_ALLOC(size) memalloc(size)
#define ZN_REALLOC(p, size) memrealloc(p, size)
#define ZN_FREE(p) memfree(p)

#endif

namespace zylann {

// Default, engine-agnostic implementation of unique pointers for this project. Allows to change it in one place.
// Note: array allocations are not used at the moment. Containers are preferred.

template <typename T>
struct DefaultObjectDeleter {
	constexpr DefaultObjectDeleter() noexcept = default;

	// This is required so we can implicitely convert from `UniquePtr<Derived>` to `UniquePtr<Base>`.
	// Looked it up from inside MSVC's STL implementation.
	template <class U, std::enable_if_t<std::is_convertible_v<U *, T *>, int> = 0>
	DefaultObjectDeleter(const DefaultObjectDeleter<U> &) noexcept {}

	inline void operator()(T *obj) {
		ZN_DELETE(obj);
	}
};

template <typename T>
using UniquePtr = std::unique_ptr<T, DefaultObjectDeleter<T>>;

template <class T, class... Types, std::enable_if_t<!std::is_array_v<T>, int> = 0>
UniquePtr<T> make_unique_instance(Types &&...args) {
	return UniquePtr<T>(ZN_NEW(T(std::forward<Types>(args)...)));
}

// Default, engine-agnostic implementation of shared pointers for this project.

template <class T, class... Types, std::enable_if_t<!std::is_array_v<T>, int> = 0>
inline std::shared_ptr<T> make_shared_instance(Types &&...args) {
	return std::shared_ptr<T>(ZN_NEW(T(std::forward<Types>(args)...)), [](T *p) { ZN_DELETE(p); });
}

} // namespace zylann

#endif // ZYLANN_MEMORY_H
