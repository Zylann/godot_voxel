#ifndef HEADER_VOXEL_UTILITY_H
#define HEADER_VOXEL_UTILITY_H

#include <core/pool_vector.h>
#include <core/vector.h>
#include <utility>
#include <vector>

#ifdef DEBUG_ENABLED
#include <core/error_macros.h>
#endif

// Takes elements starting from a given position and moves them at the beginning,
// then shrink the array to fit them. Other elements are discarded.
template <typename T>
void shift_up(Vector<T> &v, unsigned int pos) {
	unsigned int j = 0;
	for (unsigned int i = pos; i < (unsigned int)v.size(); ++i, ++j) {
		v.write[j] = v[i];
	}
	int remaining = v.size() - pos;
	v.resize(remaining);
}

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
void unordered_remove(Vector<T> &v, unsigned int pos) {
	int last = v.size() - 1;
	v.write[pos] = v[last];
	v.resize(last);
}

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
inline void unordered_remove_value(std::vector<T> &vec, T v) {
	for (size_t i = 0; i < vec.size(); ++i) {
		if (vec[i] == v) {
			vec[i] = vec.back();
			vec.pop_back();
			break;
		}
	}
}

template <typename T>
inline void append_array(std::vector<T> &dst, const std::vector<T> &src) {
	dst.insert(dst.end(), src.begin(), src.end());
}

// Removes all items satisfying the given predicate.
// This can reduce the size of the container. Items are moved to preserve order.
//template <typename T, typename F>
//inline void remove_if(std::vector<T> &vec, F predicate) {
//	unsigned int j = 0;
//	for (unsigned int i = 0; i < vec.size(); ++i) {
//		if (predicate(vec[i])) {
//			continue;
//		} else {
//			if (i != j) {
//				vec[j] = vec[i];
//			}
//			++j;
//		}
//	}
//	vec.resize(j);
//}

template <typename T>
void copy_to(PoolVector<T> &to, const Vector<T> &from) {
	to.resize(from.size());
	typename PoolVector<T>::Write w = to.write();
	for (unsigned int i = 0; i < from.size(); ++i) {
		w[i] = from[i];
	}
}

inline String ptr2s(const void *p) {
	return String::num_uint64((uint64_t)p, 16);
}

template <typename T>
void raw_copy_to(PoolVector<T> &to, const std::vector<T> &from) {
	to.resize(from.size());
	typename PoolVector<T>::Write w = to.write();
	memcpy(w.ptr(), from.data(), from.size() * sizeof(T));
}

template <typename T>
inline void sort(T &a, T &b) {
	if (a > b) {
		std::swap(a, b);
	}
}

template <typename T>
inline void sort(T &a, T &b, T &c, T &d) {
	sort(a, b);
	sort(c, d);
	sort(a, c);
	sort(b, d);
	sort(b, c);
}

// Tests if POD items in an array are all the same.
// Better tailored for more than hundred items that have power-of-two size.
template <typename Item_T>
inline bool is_uniform(const Item_T *p_data, uint32_t item_count) {
	const Item_T v0 = p_data[0];

	//typedef size_t Bucket_T;
	struct Bucket_T {
		size_t a;
		size_t b;
		inline bool operator!=(const Bucket_T &other) const {
			return a != other.a || b != other.b;
		}
	};

	if (sizeof(Bucket_T) > sizeof(Item_T) && sizeof(Bucket_T) % sizeof(Item_T) == 0) {
		static const unsigned int ITEMS_PER_BUCKET = sizeof(Bucket_T) / sizeof(Item_T);

		// Make a reference bucket
		union {
			Bucket_T packed_items;
			Item_T items[ITEMS_PER_BUCKET];
		} reference_bucket;
		for (unsigned int i = 0; i < ITEMS_PER_BUCKET; ++i) {
			reference_bucket.items[i] = v0;
		}

		// Compare using buckets of items rather than individual items
		const unsigned int bucket_count = item_count / ITEMS_PER_BUCKET;
		const Bucket_T *buckets = (const Bucket_T *)p_data;
		for (unsigned int i = 0; i < bucket_count; ++i) {
			if (buckets[i] != reference_bucket.packed_items) {
				return false;
			}
		}

		// Compare last elements individually if they don't fit in a bucket
		const unsigned int remaining_items_start = item_count - (item_count % ITEMS_PER_BUCKET);
		for (unsigned int i = remaining_items_start; i < item_count; ++i) {
			if (p_data[i] != v0) {
				return false;
			}
		}

	} else {
		for (unsigned int i = 1; i < item_count; ++i) {
			if (p_data[i] != v0) {
				return false;
			}
		}
	}

	return true;
}

#endif // HEADER_VOXEL_UTILITY_H
