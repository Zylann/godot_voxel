#ifndef ZN_GODOT_TYPED_ARRAY_H
#define ZN_GODOT_TYPED_ARRAY_H

#if defined(ZN_GODOT)
#include <core/variant/typed_array.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/variant/typed_array.hpp>
#endif

#include "../../span.h"

namespace zylann {

template <typename T>
inline void copy_to(TypedArray<T> &dst, Span<const T> src) {
	dst.resize(src.size());
	for (unsigned int i = 0; i < src.size(); ++i) {
		dst[i] = src[i];
	}
}

template <typename T>
inline void copy_to(TypedArray<T> &dst, Span<const Ref<T>> src) {
	dst.resize(src.size());
	for (unsigned int i = 0; i < src.size(); ++i) {
		dst[i] = src[i];
	}
}

template <typename T>
inline void copy_to(std::vector<T> &dst, const TypedArray<T> &src) {
	dst.resize(src.size());
	for (int i = 0; i < src.size(); ++i) {
		dst[i] = src[i];
	}
}

template <typename T>
inline void copy_to(std::vector<Ref<T>> &dst, const TypedArray<T> &src) {
	dst.resize(src.size());
	for (int i = 0; i < src.size(); ++i) {
		dst[i] = src[i];
	}
}

template <typename T>
inline TypedArray<T> to_typed_array(Span<const T> src) {
	TypedArray<T> array;
	copy_to(array, src);
	return array;
}

template <typename T>
inline TypedArray<T> to_typed_array(Span<const Ref<T>> src) {
	TypedArray<T> array;
	copy_to(array, src);
	return array;
}

} // namespace zylann

#endif // ZN_GODOT_TYPED_ARRAY_H
