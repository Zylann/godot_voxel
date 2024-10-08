#ifndef ZN_FIXED_ARRAY_H
#define ZN_FIXED_ARRAY_H

#include "../errors.h"
#include "span.h"

namespace zylann {

// TODO Could use std::array, but due to how Godot compiles,
// I couldn't find a way to enable boundary checks without failing to link my module with the rest of Godot...
// See https://github.com/godotengine/godot/issues/31608
template <typename T, unsigned int N>
class FixedArray {
public:
	// TODO Optimization: move semantics

	inline T &operator[](unsigned int i) {
#ifdef DEBUG_ENABLED
		ZN_ASSERT(i < N);
#endif
		return _data[i];
	}

	inline const T &operator[](unsigned int i) const {
#ifdef DEBUG_ENABLED
		ZN_ASSERT(i < N);
#endif
		return _data[i];
	}

	inline bool equals(const FixedArray<T, N> &other) const {
		for (unsigned int i = 0; i < N; ++i) {
			if (_data[i] != other._data[i]) {
				return false;
			}
		}
		return true;
	}

	inline bool operator==(const FixedArray<T, N> &other) const {
		return equals(other);
	}

	inline bool operator!=(const FixedArray<T, N> &other) const {
		return !equals(other);
	}

	inline constexpr T *data() {
		return _data;
	}

	inline constexpr const T *data() const {
		return _data;
	}

#if defined(__GNUC__)
	// Tells GCC that this function only depends on its arguments (none) and doesn't access `this`.
	// Workarounds calls to `size()` that sometimes produce a pedantic `maybe-uninitialized` warning,
	// despite nothing needing initialization, and `this` not even being accessed.
	__attribute__((const))
#endif
	inline constexpr unsigned int
	size() const {
		return N;
	}

	class ConstIterator {
	public:
		inline ConstIterator(const T *p) : _current(p) {}

		inline const T &operator*() {
			return *_current;
		}

		inline const T *operator->() {
			return _current;
		}

		inline ConstIterator &operator++() {
			++_current;
			return *this;
		}

		inline bool operator==(const ConstIterator other) const {
			return _current == other._current;
		}

		inline bool operator!=(const ConstIterator other) const {
			return _current != other._current;
		}

	private:
		const T *_current;
	};

	inline ConstIterator begin() const {
		return ConstIterator(_data);
	}

	inline ConstIterator end() const {
		return ConstIterator(_data + N);
	}

	class Iterator {
	public:
		inline Iterator(T *p) : _current(p) {}

		inline T &operator*() {
			return *_current;
		}

		inline T *operator->() {
			return _current;
		}

		inline Iterator &operator++() {
			++_current;
			return *this;
		}

		inline bool operator==(Iterator other) const {
			return _current == other._current;
		}

		inline bool operator!=(Iterator other) const {
			return _current != other._current;
		}

	private:
		T *_current;
	};

	inline Iterator begin() {
		return Iterator(_data);
	}

	inline Iterator end() {
		return Iterator(_data + N);
	}

private:
	T _data[N];
};

// Fills array with the same value.
// Not a method because it would not compile with non-copyable types.
template <typename T, unsigned int N>
inline void fill(FixedArray<T, N> &dst, const T v) {
	for (unsigned int i = 0; i < dst.size(); ++i) {
		dst[i] = v;
	}
}

template <typename T, unsigned int N>
inline bool find(const FixedArray<T, N> &a, const T &v, unsigned int &out_index) {
	for (unsigned int i = 0; i < a.size(); ++i) {
		if (a[i] == v) {
			out_index = i;
			return true;
		}
	}
	return false;
}

template <typename T, unsigned int N>
inline bool contains(const FixedArray<T, N> &a, const T &value_to_search) {
	for (const T &v : a) {
		if (v == value_to_search) {
			return true;
		}
	}
	return false;
}

template <typename T, unsigned int N>
Span<T> to_span(FixedArray<T, N> &a) {
	return Span<T>(a.data(), a.size());
}

template <typename T, unsigned int N>
Span<const T> to_span(const FixedArray<T, N> &a) {
	return Span<const T>(a.data(), a.size());
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

} // namespace zylann

#endif // ZN_FIXED_ARRAY_H
