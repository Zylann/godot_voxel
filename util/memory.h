#ifndef ZYLANN_MEMORY_H
#define ZYLANN_MEMORY_H

#include <memory>

namespace zylann {

// Default implementation of unique pointers for this project. Allows to change it in one place.
// Note: array allocations are not used. Containers are preferred.
// TODO Use Godot's allocator?

template <typename T>
using UniquePtr = std::unique_ptr<T>;

template <class T, class... Types, std::enable_if_t<!std::is_array_v<T>, int> = 0>
UniquePtr<T> make_unique_instance(Types &&...args) {
	return std::unique_ptr<T>(new T(std::forward<Types>(args)...));
}

} // namespace zylann

#endif // ZYLANN_MEMORY_H
