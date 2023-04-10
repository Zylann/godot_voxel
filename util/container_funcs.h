#ifndef ZN_CONTAINER_FUNCS_H
#define ZN_CONTAINER_FUNCS_H

#include "span.h"
#include <cstdint>
#include <vector>

namespace zylann {

// Takes elements starting from a given position and moves them at the beginning,
// then shrink the array to fit them. Other elements are discarded.
template <typename T>
void shift_up(std::vector<T> &v, unsigned int pos) {
	unsigned int j = 0;
	for (unsigned int i = pos; i < v.size(); ++i, ++j) {
		v[j] = v[i];
	}
	int remaining = v.size() - pos;
	v.resize(remaining);
}

// Pops the last element of the vector and place it at the given position.
// (The element that was at this position is the one removed).
template <typename T>
void unordered_remove(std::vector<T> &v, unsigned int pos) {
	v[pos] = v.back();
	v.pop_back();
}

// Removes all items satisfying the given predicate.
// This can change the size of the container, and original order of items is not preserved.
template <typename T, typename F>
inline void unordered_remove_if(std::vector<T> &vec, F predicate) {
	for (unsigned int i = 0; i < vec.size(); ++i) {
		if (predicate(vec[i])) {
			vec[i] = vec.back();
			vec.pop_back();
			// Note: can underflow, but it should be fine since it's incremented right after.
			// TODO Use a while()?
			--i;
		}
	}
}

template <typename T>
inline bool unordered_remove_value(std::vector<T> &vec, T v) {
	for (size_t i = 0; i < vec.size(); ++i) {
		if (vec[i] == v) {
			vec[i] = vec.back();
			vec.pop_back();
			return true;
		}
	}
	return false;
}

template <typename T>
inline void append_array(std::vector<T> &dst, const std::vector<T> &src) {
	dst.insert(dst.end(), src.begin(), src.end());
}

/*
// Removes all items satisfying the given predicate.
// This can reduce the size of the container. Items are moved to preserve order.
// More direct option than `vec.erase(std::remove_if(vec.begin(), vec.end(), predicate), vec.end())`.
template <typename T, typename F>
inline void remove_if(std::vector<T> &vec, F predicate) {
	unsigned int i = 0;
	unsigned int j = 0;
	for (; i < vec.size(); ++i) {
		if (predicate(vec[i])) {
			++i;
			break;
		}
	}
	for (; i < vec.size(); ++i) {
		if (predicate(i)) {
			continue;
		} else {
			vec[j] = vec[i];
			++j;
		}
	}
	vec.resize(j);
}
*/

template <typename T>
size_t find_duplicate(Span<const T> items) {
	for (unsigned int i = 0; i < items.size(); ++i) {
		const T &a = items[i];
		for (unsigned int j = i + 1; j < items.size(); ++j) {
			if (items[j] == a) {
				return j;
			}
		}
	}
	return items.size();
}

template <typename T>
bool has_duplicate(Span<const T> items) {
	return find_duplicate(items) != items.size();
}

// Tests if POD items in an array are all the same.
// Better tailored for more than hundred items that have power-of-two size.
template <typename Item_T>
inline bool is_uniform(const Item_T *p_data, size_t item_count) {
	const Item_T v0 = p_data[0];

	// typedef size_t Bucket_T;
	struct Bucket_T {
		size_t a;
		size_t b;
		inline bool operator!=(const Bucket_T &other) const {
			return a != other.a || b != other.b;
		}
	};

	if (sizeof(Bucket_T) > sizeof(Item_T) && sizeof(Bucket_T) % sizeof(Item_T) == 0) {
		static const size_t ITEMS_PER_BUCKET = sizeof(Bucket_T) / sizeof(Item_T);

		// Make a reference bucket
		union {
			Bucket_T packed_items;
			Item_T items[ITEMS_PER_BUCKET];
		} reference_bucket;
		for (size_t i = 0; i < ITEMS_PER_BUCKET; ++i) {
			reference_bucket.items[i] = v0;
		}

		// Compare using buckets of items rather than individual items
		const size_t bucket_count = item_count / ITEMS_PER_BUCKET;
		const Bucket_T *buckets = (const Bucket_T *)p_data;
		for (size_t i = 0; i < bucket_count; ++i) {
			if (buckets[i] != reference_bucket.packed_items) {
				return false;
			}
		}

		// Compare last elements individually if they don't fit in a bucket
		const size_t remaining_items_start = item_count - (item_count % ITEMS_PER_BUCKET);
		for (size_t i = remaining_items_start; i < item_count; ++i) {
			if (p_data[i] != v0) {
				return false;
			}
		}

	} else {
		for (size_t i = 1; i < item_count; ++i) {
			if (p_data[i] != v0) {
				return false;
			}
		}
	}

	return true;
}

void print_data_hex(Span<const uint8_t> data);

} // namespace zylann

#endif // ZN_CONTAINER_FUNCS_H
