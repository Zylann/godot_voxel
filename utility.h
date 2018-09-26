#ifndef HEADER_VOXEL_UTILITY_H
#define HEADER_VOXEL_UTILITY_H

#include <core/vector.h>
#include <core/dvector.h>
#include <core/ustring.h>
#include "vector3i.h"

// Takes elements starting from a given position and moves them at the beginning,
// then shrink the array to fit them. Other elements are discarded.
template <typename T>
void shift_up(Vector<T> &v, int pos) {

	int j = 0;
	for (int i = pos; i < v.size(); ++i, ++j) {
		v.write[j] = v[i];
	}

	int remaining = v.size() - pos;
	v.resize(remaining);
}

// Pops the last element of the vector and place it at the given position.
// (The element that was at this position is the one removed).
template <typename T>
void unordered_remove(Vector<T> &v, int pos) {
	int last = v.size() - 1;
	v.write[pos] = v[last];
	v.resize(last);
}

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

#endif // HEADER_VOXEL_UTILITY_H
