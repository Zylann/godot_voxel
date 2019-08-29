#ifndef HEADER_VOXEL_UTILITY_H
#define HEADER_VOXEL_UTILITY_H

#include <core/pool_vector.h>
#include <core/ustring.h>
#include <core/vector.h>
#include <scene/resources/mesh.h>
#include <vector>

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

// Trilinear interpolation between corner values of a cube.
// Cube points respect the same position as in octree_tables.h
template <typename T>
inline T interpolate(const T v0, const T v1, const T v2, const T v3, const T v4, const T v5, const T v6, const T v7, Vector3 position) {

	const float one_min_x = 1.f - position.x;
	const float one_min_y = 1.f - position.y;
	const float one_min_z = 1.f - position.z;
	const float one_min_x_one_min_y = one_min_x * one_min_y;
	const float x_one_min_y = position.x * one_min_y;

	T res = one_min_z * (v0 * one_min_x_one_min_y + v1 * x_one_min_y + v4 * one_min_x * position.y);
	res += position.z * (v3 * one_min_x_one_min_y + v2 * x_one_min_y + v7 * one_min_x * position.y);
	res += position.x * position.y * (v5 * one_min_z + v6 * position.z);

	return res;
}

inline float min(const float &a, const float &b) {
	return a < b ? a : b;
}

inline float max(const float &a, const float &b) {
	return a > b ? a : b;
}

inline bool is_surface_triangulated(Array surface) {
	PoolVector3Array positions = surface[Mesh::ARRAY_VERTEX];
	PoolIntArray indices = surface[Mesh::ARRAY_INDEX];
	return positions.size() >= 3 && indices.size() >= 3;
}

inline bool is_mesh_empty(Ref<Mesh> mesh_ref) {
	if (mesh_ref.is_null())
		return true;
	const Mesh &mesh = **mesh_ref;
	if (mesh.get_surface_count() == 0)
		return true;
	if (mesh.surface_get_array_len(0) == 0)
		return true;
	return false;
}

template <typename T>
inline void append_array(std::vector<T> &dst, const std::vector<T> &src) {
	dst.insert(dst.end(), src.begin(), src.end());
}

inline int udiv(int x, int d) {
	if (x < 0) {
		return (x - d + 1) / d;
	} else {
		return x / d;
	}
}

// `Math::wrapi` with zero min
inline int wrap(int x, int d) {
	return ((x % d) + d) % d;
}

#if TOOLS_ENABLED
namespace VoxelDebug {
void create_debug_box_mesh();
void free_debug_box_mesh();
Ref<Mesh> get_debug_box_mesh();
}
#endif

#endif // HEADER_VOXEL_UTILITY_H
