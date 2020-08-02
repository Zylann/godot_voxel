#ifndef HEADER_VOXEL_UTILITY_H
#define HEADER_VOXEL_UTILITY_H

#include <core/math/vector3.h>
#include <core/pool_vector.h>
#include <core/reference.h>
#include <core/vector.h>
#include <vector>

class Mesh;
class Object;

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

// TODO Move math funcs under math/ folder and wrap them in a namespace

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

template <typename T>
inline T min(const T a, const T b) {
	return a < b ? a : b;
}

template <typename T>
inline T max(const T a, const T b) {
	return a > b ? a : b;
}

template <typename T>
inline T min(const T a, const T b, const T c, const T d) {
	return min(min(a, b), min(c, d));
}

template <typename T>
inline T max(const T a, const T b, const T c, const T d) {
	return max(max(a, b), max(c, d));
}

template <typename T>
inline T clamp(const T x, const T min_value, const T max_value) {
	if (x < min_value) {
		return min_value;
	}
	if (x >= max_value) {
		return max_value;
	}
	return x;
}

template <typename T>
inline T squared(const T x) {
	return x * x;
}

template <typename T>
inline void sort_min_max(T &a, T &b) {
	if (a > b) {
		T temp = a;
		a = b;
		b = temp;
	}
}

bool is_surface_triangulated(Array surface);
bool is_mesh_empty(Ref<Mesh> mesh_ref);

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

// TODO Rename `wrapi`
// `Math::wrapi` with zero min
inline int wrap(int x, int d) {
	return ((unsigned int)x - (x < 0)) % (unsigned int)d;
	//return ((x % d) + d) % d;
}

// Math::wrapf with zero min
inline float wrapf(float x, float d) {
	return Math::is_zero_approx(d) ? 0.f : x - (d * Math::floor(x / d));
}

// Similar to Math::smoothstep but doesn't use macro to clamp
inline float smoothstep(float p_from, float p_to, float p_weight) {
	if (Math::is_equal_approx(p_from, p_to)) {
		return p_from;
	}
	float x = clamp((p_weight - p_from) / (p_to - p_from), 0.0f, 1.0f);
	return x * x * (3.0f - 2.0f * x);
}

bool try_call_script(const Object *obj, StringName method_name, const Variant **args, unsigned int argc, Variant *out_ret);

inline bool try_call_script(const Object *obj, StringName method_name, Variant arg0, Variant arg1, Variant arg2, Variant *out_ret) {
	const Variant *args[3] = { &arg0, &arg1, &arg2 };
	return try_call_script(obj, method_name, args, 3, out_ret);
}

#if TOOLS_ENABLED
namespace VoxelDebug {
void create_debug_box_mesh();
void free_debug_box_mesh();
Ref<Mesh> get_debug_box_mesh();
} // namespace VoxelDebug
#endif

#endif // HEADER_VOXEL_UTILITY_H
