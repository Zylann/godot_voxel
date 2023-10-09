#ifndef ZN_SMALL_VECTOR_H
#define ZN_SMALL_VECTOR_H

#include "errors.h"
#include <memory>
#include <new>
#include <type_traits>

namespace zylann {

// Dynamic sequence of elements using fixed capacity.
// Meant for small amount of elements, where their maximum count is also known and small.
// May be preferred to dynamic capacity vectors for performance, since no heap allocation occurs. It is not a drop-in
// replacement, as it does not support adding elements over capacity.
template <typename T, unsigned int N>
class SmallVector {
public:
	~SmallVector() {
		for (unsigned int i = 0; i < _size; ++i) {
			std::destroy_at(std::launder(reinterpret_cast<T *>(&_data[i])));
		}
	}

	inline void push_back(const T &v) {
		ZN_ASSERT(_size < capacity());
		::new (&_data[_size]) T(v);
		++_size;
	}

	inline void clear() {
		// Destroy all elements
		for (unsigned int i = 0; i < _size; ++i) {
			std::destroy_at(std::launder(reinterpret_cast<T *>(&_data[i])));
		}
		_size = 0;
	}

	void resize(unsigned int new_size) {
		if (_size == new_size) {
			return;
		}

		ZN_ASSERT(new_size <= capacity());

		// Default-construct new elements
		for (; _size < new_size; ++_size) {
			::new (&_data[_size]) T();
		}

		// Destroy excess elements
		for (; _size > new_size; --_size) {
			std::destroy_at(std::launder(reinterpret_cast<T *>(&_data[_size])));
		}
	}

	void resize(unsigned int new_size, const T &default_value) {
		if (_size == new_size) {
			return;
		}

		ZN_ASSERT(new_size <= capacity());

		// Copy-construct new elements
		for (; _size < new_size; ++_size) {
			::new (&_data[_size]) T(default_value);
		}

		// Destroy excess elements
		for (; _size > new_size; --_size) {
			std::destroy_at(std::launder(reinterpret_cast<T *>(&_data[_size])));
		}
	}

	inline unsigned int size() const {
		return _size;
	}

	inline unsigned int capacity() const {
		return N;
	}

	inline T &operator[](unsigned int i) {
		return *std::launder(reinterpret_cast<T *>(&_data[i]));
	}

	inline const T &operator[](unsigned int i) const {
		return *std::launder(reinterpret_cast<const T *>(&_data[i]));
	}

	SmallVector<T, N> &operator=(const SmallVector<T, N> &other) {
		clear();

		for (; _size < other.size(); ++_size) {
			::new (&_data[_size]) T(other[_size]);
		}

		return *this;
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
		return Iterator(std::launder(reinterpret_cast<T *>(&_data[0])));
	}

	inline Iterator end() {
		return Iterator(std::launder(reinterpret_cast<T *>(&_data[_size])));
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
		return ConstIterator(std::launder(reinterpret_cast<const T *>(&_data[0])));
	}

	inline ConstIterator end() const {
		return ConstIterator(std::launder(reinterpret_cast<const T *>(&_data[_size])));
	}

private:
	// Logical number of elements. Smaller or equal to N.
	// Would it be worth using uint16_t or uint8_t when N is small enough?
	unsigned int _size = 0;

	// https://en.cppreference.com/w/cpp/types/aligned_storage
	std::aligned_storage_t<sizeof(T), alignof(T)> _data[N];
};

} // namespace zylann

#endif // ZN_SMALL_VECTOR_H
