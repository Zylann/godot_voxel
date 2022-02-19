#ifndef FLAT_MAP_H
#define FLAT_MAP_H

#include "span.h"
#include <algorithm>
#include <vector>

namespace zylann {

template <typename T>
struct FlatMapDefaultComparator {
	static inline bool less_than(const T &a, const T &b) {
		return a < b;
	}
};

// Associative container based on a sorted internal vector.
// Should be a good tradeoff for small amount of unique items with key lookup while keeping fast iteration.
// Two elements with the same key is not allowed.
// The address of keys and values is not guaranteed to be stable.
// See https://riptutorial.com/cplusplus/example/7270/using-a-sorted-vector-for-fast-element-lookup
template <typename K, typename T, typename KComp = FlatMapDefaultComparator<K>>
class FlatMap {
public:
	struct Pair {
		K key;
		T value;

		// For std::sort
		inline bool operator<(const Pair &other) const {
			return KComp::less_than(key, other.key);
		}

		// For std::lower_bound
		inline bool operator<(const K &other_key) const {
			return KComp::less_than(key, other_key);
		}
	};

	// If the key already exists, the item is not inserted and returns false.
	// If insertion was successful, returns true.
	bool insert(K key, T value) {
		typename std::vector<Pair>::const_iterator it = std::lower_bound(_items.begin(), _items.end(), key);
		if (it != _items.end() && it->key == key) {
			return false;
		}
		_items.insert(it, Pair{ key, value });
		return true;
	}

	// If the key already exists, the item will replace the previous value.
	void insert_or_assign(K key, T value) {
		typename std::vector<Pair>::iterator it = std::lower_bound(_items.begin(), _items.end(), key);
		if (it != _items.end() && it->key == key) {
			it->value = value;
		} else {
			_items.insert(it, Pair{ key, value });
		}
	}

	// Initialize from a collection if items.
	// Faster than doing individual insertion of each item.
	void clear_and_insert(Span<Pair> pairs) {
		clear();
		_items.resize(pairs.size());
		for (size_t i = 0; i < pairs.size(); ++i) {
			_items[i] = pairs[i];
		}
		std::sort(_items.begin(), _items.end());
	}

	bool find(K key, T &out_value) const {
		typename std::vector<Pair>::const_iterator it = std::lower_bound(_items.begin(), _items.end(), key);
		if (it != _items.end() && it->key == key) {
			out_value = it->value;
			return true;
		}
		return false;
	}

	bool has(K key) const {
		// Using std::binary_search is very annoying.
		// First, we don't want to pass a Pair because it would require constructing a T.
		// Using just the key as the "value" to search doesn't compile because Pair only has comparison with K as second
		// argument.
		// Specifying a comparison lambda is also not viable because both arguments need to be convertible to K.
		// Making it work would require passing a struct with two operators() with arguments in both orders...
		//return std::binary_search(_items.cbegin(), _items.cend(), key);

		typename std::vector<Pair>::const_iterator it = std::lower_bound(_items.begin(), _items.end(), key);
		return it != _items.end() && it->key == key;
	}

	bool erase(K key) {
		typename std::vector<Pair>::iterator it = std::lower_bound(_items.begin(), _items.end(), key);
		if (it != _items.end() && it->key == key) {
			_items.erase(it);
			return true;
		}
		return false;
	}

	inline size_t size() const {
		return _items.size();
	}

	void clear() {
		_items.clear();
	}

	// template <typename F>
	// inline void for_each(F f) {
	// 	for (auto it = _items.begin(); it != _items.end(); ++it) {
	// 		// Do not expose the possibility of modifying keys
	// 		const K key = it->key;
	// 		f(key, it->value);
	// 	}
	// }

	// template <typename F>
	// inline void for_each_const(F f) const {
	// 	for (auto it = _items.begin(); it != _items.end(); ++it) {
	// 		// Do not expose the possibility of modifying keys
	// 		const K key = it->key;
	// 		f(key, it->value);
	// 	}
	// }

	// `bool predicate(FlatMap<K, T>::Pair)`
	template <typename F>
	inline void remove_if(F predicate) {
		_items.erase(std::remove_if(_items.begin(), _items.end(), predicate));
	}

	void operator=(const FlatMap<K, T> &other) {
		_items = other._items;
	}

	bool operator==(const FlatMap<K, T> &other) const {
		return _items == other._items;
	}

	class ConstIterator {
	public:
		ConstIterator(const Pair *p) : _current(p) {}

		inline const Pair &operator*() {
#ifdef DEBUG_ENABLED
			CRASH_COND(_current == nullptr);
#endif
			return *_current;
		}

		inline const Pair *operator->() {
#ifdef DEBUG_ENABLED
			CRASH_COND(_current == nullptr);
#endif
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
		const Pair *_current;
	};

	inline ConstIterator begin() const {
		return ConstIterator(_items.empty() ? nullptr : &_items[0]);
	}

	inline ConstIterator end() const {
		return ConstIterator(_items.empty() ? nullptr : (&_items[0] + _items.size()));
	}

private:
	// Sorted by key
	std::vector<Pair> _items;
};

} // namespace zylann

#endif // FLAT_MAP_H
