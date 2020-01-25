#ifndef ARRAY_SLICE_H
#define ARRAY_SLICE_H

#include "fixed_array.h"
#include <core/error_macros.h>
#include <vector>

// View into an array, referencing a pointer and a size.
template <typename T>
class ArraySlice {
public:
	// TODO Get rid of unsafe constructor, use specialized ones
	inline ArraySlice(T *p_ptr, size_t p_begin, size_t p_end) {
		CRASH_COND(p_end <= p_begin);
		_ptr = p_ptr + p_begin;
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

	inline T &operator[](size_t i) {
#ifdef TOOLS_ENABLED
		CRASH_COND(i >= _size)
#endif
		return _ptr[i];
	}

	inline const T &operator[](size_t i) const {
#ifdef TOOLS_ENABLED
		CRASH_COND(i >= _size)
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

private:
	T *_ptr;
	size_t _size;
};

#endif // ARRAY_SLICE_H
