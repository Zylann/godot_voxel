#ifndef ARRAY_SLICE_H
#define ARRAY_SLICE_H

#include "fixed_array.h"
#include <core/error_macros.h>

template <typename T>
class ArraySlice {
public:
	inline ArraySlice(T *p_ptr, size_t p_begin, size_t p_end) {
		CRASH_COND(p_end <= p_begin);
		_ptr = p_ptr + p_begin;
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
