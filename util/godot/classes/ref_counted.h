#ifndef ZN_GODOT_REF_COUNTED_H
#define ZN_GODOT_REF_COUNTED_H

#include <functional>

#if defined(ZN_GODOT)
#include <core/object/ref_counted.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/ref_counted.hpp>
using namespace godot;
#endif

namespace zylann {

// `(ref1 = ref2).is_valid()` does not work because Ref<T> does not implement an `operator=` returning the value.
// So instead we can write it as `try_get_as(ref2, ref1)`
template <typename From_T, typename To_T>
inline bool try_get_as(const Ref<From_T> &from, Ref<To_T> &to) {
	to = from;
	return to.is_valid();
}

// To allow using Ref<T> as key in Godot's HashMap
template <typename T>
struct RefHasher {
	static _FORCE_INLINE_ uint32_t hash(const Ref<T> &v) {
		return uint32_t(uint64_t(v.ptr())) * (0x9e3779b1L);
	}
};

} // namespace zylann

namespace std {

// For Ref<T> keys in std::unordered_map, hashed by pointer, not by content
template <typename T>
struct hash<Ref<T>> {
	inline size_t operator()(const Ref<T> &v) const {
		return std::hash<const T *>{}(v.ptr());
	}
};

} // namespace std

#endif // ZN_GODOT_REF_COUNTED_H
