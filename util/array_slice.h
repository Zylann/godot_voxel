#ifndef ARRAY_SLICE_H
#define ARRAY_SLICE_H

#include "fixed_array.h"
#include <core/error_macros.h>
#include <vector>

// View into an array, referencing a pointer and a size.
template <typename T>
class ArraySlice {
public:
	inline ArraySlice() :
			_ptr(nullptr),
			_size(0) {
	}

	// TODO Get rid of unsafe constructor, use specialized ones
	inline ArraySlice(T *p_ptr, size_t p_begin, size_t p_end) {
		CRASH_COND(p_end <= p_begin);
		_ptr = p_ptr + p_begin;
		_size = p_end - p_begin;
	}

	inline ArraySlice(T *p_ptr, size_t p_size) :
			_ptr(p_ptr), _size(p_size) {}

	inline ArraySlice(ArraySlice<T> &p_other, size_t p_begin, size_t p_end) {
		CRASH_COND(p_end <= p_begin);
		CRASH_COND(p_begin >= p_other.size());
		CRASH_COND(p_end > p_other.size()); // `>` because p_end is typically `p_begin + size`
		_ptr = p_other._ptr + p_begin;
		_size = p_end - p_begin;
	}

	inline ArraySlice(std::vector<T> &vec, size_t p_begin, size_t p_end) {
		CRASH_COND(p_end <= p_begin);
		CRASH_COND(p_begin >= vec.size());
		CRASH_COND(p_end > vec.size()); // `>` because p_end is typically `p_begin + size`
		_ptr = &vec[p_begin];
		_size = p_end - p_begin;
	}

	template <unsigned int N>
	inline ArraySlice(FixedArray<T, N> &a) {
		_ptr = a.data();
		_size = a.size();
	}

	inline ArraySlice<T> sub(size_t from, size_t len) const {
		CRASH_COND(from + len >= _size);
		return ArraySlice<T>(_ptr + from, len);
	}

	inline ArraySlice<T> sub(size_t from) const {
		CRASH_COND(from >= _size);
		return ArraySlice<T>(_ptr + from, _size - from);
	}

	// const ArraySlice<T> sub_const(size_t from, size_t len) const {
	// 	CRASH_COND(from + len >= _size);
	// 	return ArraySlice{ _ptr + from, len };
	// }

	// const ArraySlice<T> sub_const(size_t from) const {
	// 	CRASH_COND(from >= _size);
	// 	return ArraySlice{ _ptr + from, _size - from };
	// }

	template <typename U>
	ArraySlice<U> reinterpret_cast_to() const {
		const size_t size_in_bytes = _size * sizeof(T);
#ifdef DEBUG_ENABLED
		CRASH_COND(size_in_bytes % sizeof(U) != 0);
#endif
		return ArraySlice<U>(reinterpret_cast<U *>(_ptr), 0, size_in_bytes / sizeof(U));
	}

	inline T &operator[](size_t i) {
#ifdef DEBUG_ENABLED
		CRASH_COND(i >= _size);
#endif
		return _ptr[i];
	}

	inline const T &operator[](size_t i) const {
#ifdef DEBUG_ENABLED
		CRASH_COND(i >= _size);
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

private:
	T *_ptr;
	size_t _size;
};

#endif // ARRAY_SLICE_H
