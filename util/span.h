#ifndef ZN_SPAN_H
#define ZN_SPAN_H

#include "fixed_array.h"
#include <cstddef>
#include <vector>

namespace zylann {

// View into an array, referencing a pointer and a size.
// STL equivalent would be std::span<T> in C++20
template <typename T>
class Span {
public:
	inline Span() : _ptr(nullptr), _size(0) {}

	inline Span(T *p_ptr, size_t p_begin, size_t p_end) {
		ZN_ASSERT(p_end >= p_begin);
		_ptr = p_ptr + p_begin;
		_size = p_end - p_begin;
	}

	inline Span(T *p_ptr, size_t p_size) : _ptr(p_ptr), _size(p_size) {}

	inline Span(Span<T> &p_other, size_t p_begin, size_t p_end) {
		ZN_ASSERT(p_end >= p_begin);
		ZN_ASSERT(p_begin < p_other.size());
		ZN_ASSERT(p_end <= p_other.size()); // `<=` because p_end is typically `p_begin + size`
		_ptr = p_other._ptr + p_begin;
		_size = p_end - p_begin;
	}

	// Initially added to support `Span<const T> = Span<T>`, or `Span<Base> = Span<Derived>`
	template <typename U>
	inline Span(Span<U> p_other) {
		_ptr = p_other.data();
		_size = p_other.size();
	}

	// TODO Remove this one, prefer to_span() specializations
	inline Span(std::vector<T> &vec, size_t p_begin, size_t p_end) {
		ZN_ASSERT(p_end >= p_begin);
		ZN_ASSERT(p_begin < vec.size());
		ZN_ASSERT(p_end <= vec.size()); // `<=` because p_end is typically `p_begin + size`
		_ptr = &vec[p_begin];
		_size = p_end - p_begin;
	}

	// TODO Remove this one, prefer to_span() specializations
	template <unsigned int N>
	inline Span(FixedArray<T, N> &a) {
		_ptr = a.data();
		_size = a.size();
	}

	inline Span<T> sub(size_t from, size_t len) const {
		ZN_ASSERT(from + len <= _size);
		return Span<T>(_ptr + from, len);
	}

	inline Span<T> sub(size_t from) const {
		ZN_ASSERT(from < _size);
		return Span<T>(_ptr + from, _size - from);
	}

	template <typename U>
	Span<U> reinterpret_cast_to() const {
		const size_t size_in_bytes = _size * sizeof(T);
#ifdef DEBUG_ENABLED
		ZN_ASSERT(size_in_bytes % sizeof(U) == 0);
#endif
		return Span<U>(reinterpret_cast<U *>(_ptr), 0, size_in_bytes / sizeof(U));
	}

	inline void set(size_t i, T v) {
#ifdef DEBUG_ENABLED
		ZN_ASSERT(i < _size);
#endif
		_ptr[i] = v;
	}

	inline T &operator[](size_t i) {
#ifdef DEBUG_ENABLED
		ZN_ASSERT(i < _size);
#endif
		return _ptr[i];
	}

	inline const T &operator[](size_t i) const {
#ifdef DEBUG_ENABLED
		ZN_ASSERT(i < _size);
#endif
		return _ptr[i];
	}

	inline size_t size() const {
		return _size;
	}

	inline T *data() {
		return _ptr;
	}

	inline const T *data() const {
		return _ptr;
	}

	inline void fill(const T v) {
		const T *end = _ptr + _size;
		for (T *p = _ptr; p != end; ++p) {
			*p = v;
		}
	}

	inline bool overlaps(const Span<T> other) const {
		return _ptr + _size > other._ptr && _ptr < other._ptr + other._size;
	}

	// TODO Iterators

private:
	T *_ptr;
	size_t _size;
};

template <typename T>
Span<T> to_span(std::vector<T> &vec) {
	return Span<T>(vec.data(), 0, vec.size());
}

template <typename T>
Span<const T> to_span(const std::vector<T> &vec) {
	return Span<const T>(vec.data(), 0, vec.size());
}

template <typename T>
Span<T> to_span_from_position_and_size(std::vector<T> &vec, unsigned int pos, unsigned int size) {
	ZN_ASSERT(pos + size <= vec.size());
	return Span<T>(vec.data(), pos, pos + size);
}

template <typename T>
Span<const T> to_span_from_position_and_size(const std::vector<T> &vec, unsigned int pos, unsigned int size) {
	ZN_ASSERT(pos + size <= vec.size());
	return Span<const T>(vec.data(), pos, pos + size);
}

// TODO Deprecate, now Span has a conversion constructor that can allow doing that
template <typename T>
Span<const T> to_span_const(const std::vector<T> &vec) {
	return Span<const T>(vec.data(), 0, vec.size());
}

template <typename T, unsigned int N>
Span<T> to_span(FixedArray<T, N> &a) {
	return Span<T>(a.data(), a.size());
}

template <typename T, unsigned int N>
Span<T> to_span(FixedArray<T, N> &a, unsigned int count) {
	ZN_ASSERT(count <= a.size());
	return Span<T>(a.data(), count);
}

// TODO Deprecate, now Span has a conversion constructor that can allow doing that
template <typename T, unsigned int N>
Span<const T> to_span_const(const FixedArray<T, N> &a, unsigned int count) {
	ZN_ASSERT(count <= a.size());
	return Span<const T>(a.data(), count);
}

// TODO Deprecate, now Span has a conversion constructor that can allow doing that
template <typename T, unsigned int N>
Span<const T> to_span_const(const FixedArray<T, N> &a) {
	return Span<const T>(a.data(), 0, a.size());
}

// TODO Deprecate, now Span has a conversion constructor that can allow doing that
template <typename T>
Span<const T> to_span_const(const Span<T> &a) {
	return Span<const T>(a.data(), 0, a.size());
}

} // namespace zylann

#endif // ZN_SPAN_H
