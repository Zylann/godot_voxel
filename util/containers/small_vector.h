#ifndef ZN_SMALL_VECTOR_H
#define ZN_SMALL_VECTOR_H

#include "../errors.h"
#include "span.h"
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
	// Initially added for natvis, for some reason (T*) doesnt work
	using ValueType = T;

	// TODO Bunch of features not supported, not needed for now.
	// constexpr is not possible due to `reinterpret_cast`. However we have no need for it at the moment.

	SmallVector() = default;

	SmallVector(const SmallVector<T, N> &other) {
		while (_size < other._size) {
			::new (&_items[_size]) T(other[_size]);
			++_size;
		}
	}

	~SmallVector() {
		for (unsigned int i = 0; i < _size; ++i) {
			std::destroy_at(std::launder(reinterpret_cast<T *>(&_items[i])));
		}
	}

	inline void push_back(const T &v) {
		ZN_ASSERT(_size < capacity());
		::new (&_items[_size]) T(v);
		++_size;
	}

	inline void clear() {
		// Destroy all elements
		for (unsigned int i = 0; i < _size; ++i) {
			std::destroy_at(std::launder(reinterpret_cast<T *>(&_items[i])));
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
			::new (&_items[_size]) T();
		}

		// Destroy excess elements
		for (; _size > new_size; --_size) {
			std::destroy_at(std::launder(reinterpret_cast<T *>(&_items[_size])));
		}
	}

	void resize(unsigned int new_size, const T &default_value) {
		if (_size == new_size) {
			return;
		}

		ZN_ASSERT(new_size <= capacity());

		// Copy-construct new elements
		for (; _size < new_size; ++_size) {
			::new (&_items[_size]) T(default_value);
		}

		// Destroy excess elements
		for (; _size > new_size; --_size) {
			std::destroy_at(std::launder(reinterpret_cast<T *>(&_items[_size])));
		}
	}

	inline unsigned int size() const {
		return _size;
	}

	inline constexpr unsigned int capacity() const {
		return N;
	}

	inline const T *data() const {
		return &(*this)[0];
	}

	inline T &operator[](unsigned int i) {
#ifdef DEBUG_ENABLED
		ZN_ASSERT(i < N);
#endif
		return *std::launder(reinterpret_cast<T *>(&_items[i]));
	}

	inline const T &operator[](unsigned int i) const {
#ifdef DEBUG_ENABLED
		ZN_ASSERT(i < N);
#endif
		return *std::launder(reinterpret_cast<const T *>(&_items[i]));
	}

	SmallVector<T, N> &operator=(const SmallVector<T, N> &other) {
		if ((void *)this != (void *)&other) {
			clear();

			for (; _size < other.size(); ++_size) {
				::new (&_items[_size]) T(other[_size]);
			}
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
		return Iterator(std::launder(reinterpret_cast<T *>(&_items[0])));
	}

	inline Iterator end() {
		return Iterator(std::launder(reinterpret_cast<T *>(&_items[_size])));
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
		return ConstIterator(std::launder(reinterpret_cast<const T *>(&_items[0])));
	}

	inline ConstIterator end() const {
		return ConstIterator(std::launder(reinterpret_cast<const T *>(&_items[_size])));
	}

private:
	// The reference presents an implementation of small vector using std::aligned_storage:
	// https://en.cppreference.com/w/cpp/types/aligned_storage
	// However it is going to get deprecated in C++23
	// https://stackoverflow.com/questions/71828288/why-is-stdaligned-storage-to-be-deprecated-in-c23-and-what-to-use-instead

	struct alignas(T) Item {
		uint8_t data[sizeof(T)];
	};

	static_assert(sizeof(T) == sizeof(Item), "Mismatch in size");
	static_assert(alignof(T) == alignof(Item), "Mismatch in alignment");

	// Logical number of elements. Smaller or equal to N.
	// Would it be worth using uint16_t or uint8_t when N is small enough?
	unsigned int _size = 0;

	Item _items[N];
};

template <typename T, unsigned int N>
Span<const T> to_span(const SmallVector<T, N> &v) {
	return Span<const T>(v.data(), v.size());
}

} // namespace zylann

#endif // ZN_SMALL_VECTOR_H
