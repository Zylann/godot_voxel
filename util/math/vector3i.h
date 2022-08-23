#ifndef ZYLANN_VECTOR3I_H
#define ZYLANN_VECTOR3I_H

#include "funcs.h"

#include <core/math/vector3.h>
#include <core/math/vector3i.h>
#include <core/templates/hashfuncs.h>
#include <iosfwd>

#if VOXEL_CUSTOM_VECTOR3I
struct Vector3i {
	static const unsigned int AXIS_X = 0;
	static const unsigned int AXIS_Y = 1;
	static const unsigned int AXIS_Z = 2;
	static const unsigned int AXIS_COUNT = 3;

	union {
		struct {
			int x;
			int y;
			int z;
		};
		int coords[3];
	};

	_FORCE_INLINE_ Vector3i() : x(0), y(0), z(0) {}

	explicit _FORCE_INLINE_ Vector3i(int xyz) : x(xyz), y(xyz), z(xyz) {}

	_FORCE_INLINE_ Vector3i(int px, int py, int pz) : x(px), y(py), z(pz) {}

	_FORCE_INLINE_ Vector3i(const Vector3i &other) {
		*this = other;
	}

	static inline Vector3i from_cast(const Vector3 &f) {
		return Vector3i(f.x, f.y, f.z);
	}

	static inline Vector3i from_floored(const Vector3 &f) {
		return Vector3i(Math::floor(f.x), Math::floor(f.y), Math::floor(f.z));
	}

	static inline Vector3i from_rounded(const Vector3 &f) {
		return Vector3i(Math::round(f.x), Math::round(f.y), Math::round(f.z));
	}

	static inline Vector3i from_ceiled(const Vector3 &f) {
		return Vector3i(Math::ceil(f.x), Math::ceil(f.y), Math::ceil(f.z));
	}

	_FORCE_INLINE_ Vector3 to_vec3() const {
		return Vector3(x, y, z);
	}

	// Returning a 64-bit integer because volumes can quickly overflow INT_MAX (like 1300^3),
	// even though dense volumes of that size will rarely be encountered in this module
	_FORCE_INLINE_ int64_t volume() const {
#ifdef DEBUG_ENABLED
		ERR_FAIL_COND_V(x < 0 || y < 0 || z < 0, 0);
#endif
		return x * y * z;
	}

	// Returning a 64-bit integer because squared distances can quickly overflow INT_MAX
	_FORCE_INLINE_ int64_t length_sq() const {
		return x * x + y * y + z * z;
	}

	_FORCE_INLINE_ real_t length() const {
		return Math::sqrt(real_t(length_sq()));
	}

	_FORCE_INLINE_ int64_t distance_sq(const Vector3i &other) const;

	_FORCE_INLINE_ Vector3i &operator=(const Vector3i &other) {
		x = other.x;
		y = other.y;
		z = other.z;
		return *this;
	}

	_FORCE_INLINE_ void operator+=(const Vector3i &other) {
		x += other.x;
		y += other.y;
		z += other.z;
	}

	_FORCE_INLINE_ void operator-=(const Vector3i &other) {
		x -= other.x;
		y -= other.y;
		z -= other.z;
	}

	_FORCE_INLINE_ void operator*=(const int s) {
		x *= s;
		y *= s;
		z *= s;
	}

	_FORCE_INLINE_ Vector3i operator-() const {
		return Vector3i(-x, -y, -z);
	}

	_FORCE_INLINE_ int &operator[](unsigned int i) {
#if TOOLS_ENABLED
		CRASH_COND(i >= AXIS_COUNT);
#endif
		return coords[i];
	}

	_FORCE_INLINE_ const int &operator[](unsigned int i) const {
#if TOOLS_ENABLED
		CRASH_COND(i >= AXIS_COUNT);
#endif
		return coords[i];
	}

	// TODO Change this to be consistent with `::clamp`
	// Clamps between min and max, where max is excluded
	void clamp_to(const Vector3i min, const Vector3i max) {
		if (x < min.x) {
			x = min.x;
		}
		if (y < min.y) {
			y = min.y;
		}
		if (z < min.z) {
			z = min.z;
		}

		if (x >= max.x) {
			x = max.x - 1;
		}
		if (y >= max.y) {
			y = max.y - 1;
		}
		if (z >= max.z) {
			z = max.z - 1;
		}
	}

	// TODO Deprecate
	_FORCE_INLINE_ bool is_contained_in(const Vector3i &min, const Vector3i &max) {
		return x >= min.x && y >= min.y && z >= min.z && x < max.x && y < max.y && z < max.z;
	}

	static void sort_min_max(Vector3i &a, Vector3i &b) {
		::sort(a.x, b.x);
		::sort(a.y, b.y);
		::sort(a.z, b.z);
	}

	inline Vector3i floordiv(const Vector3i d) const {
		return Vector3i(::floordiv(x, d.x), ::floordiv(y, d.y), ::floordiv(z, d.z));
	}

	inline Vector3i floordiv(const int d) const {
		return Vector3i(::floordiv(x, d), ::floordiv(y, d), ::floordiv(z, d));
	}

	inline Vector3i ceildiv(const int d) const {
		return Vector3i(::ceildiv(x, d), ::ceildiv(y, d), ::ceildiv(z, d));
	}

	inline Vector3i wrap(const Vector3i d) const {
		return Vector3i(::wrap(x, d.x), ::wrap(y, d.y), ::wrap(z, d.z));
	}

	inline unsigned int get_zxy_index(const Vector3i area_size) const {
		return y + area_size.y * (x + area_size.x * z); // ZXY
	}

	static inline Vector3i from_zxy_index(unsigned int i, const Vector3i area_size) {
		Vector3i pos;
		pos.y = i % area_size.y;
		pos.x = (i / area_size.y) % area_size.x;
		pos.z = i / (area_size.y * area_size.x);
		return pos;
	}

	bool all_members_equal() const {
		return x == y && y == z;
	}

	inline bool is_unit_vector() const {
		return Math::abs(x) + Math::abs(y) + Math::abs(z) == 1;
	}

	static inline Vector3i min(const Vector3i a, const Vector3i b) {
		return Vector3i(::min(a.x, b.x), ::min(a.y, b.y), ::min(a.z, b.z));
	}
};

_FORCE_INLINE_ Vector3i operator+(const Vector3i &a, const Vector3i &b) {
	return Vector3i(a.x + b.x, a.y + b.y, a.z + b.z);
}

_FORCE_INLINE_ Vector3i operator-(const Vector3i &a, const Vector3i &b) {
	return Vector3i(a.x - b.x, a.y - b.y, a.z - b.z);
}

_FORCE_INLINE_ Vector3i operator*(const Vector3i &a, const Vector3i &b) {
	return Vector3i(a.x * b.x, a.y * b.y, a.z * b.z);
}

_FORCE_INLINE_ Vector3i operator*(const Vector3i &a, int n) {
	return Vector3i(a.x * n, a.y * n, a.z * n);
}

_FORCE_INLINE_ Vector3i operator*(int n, const Vector3i &a) {
	return Vector3i(a.x * n, a.y * n, a.z * n);
}

_FORCE_INLINE_ Vector3i operator/(const Vector3i &a, int n) {
	return Vector3i(a.x / n, a.y / n, a.z / n);
}

_FORCE_INLINE_ Vector3i operator/(const Vector3i &a, const Vector3i &d) {
	return Vector3i(a.x / d.x, a.y / d.y, a.z / d.z);
}

_FORCE_INLINE_ bool operator==(const Vector3i &a, const Vector3i &b) {
	return a.x == b.x && a.y == b.y && a.z == b.z;
}

_FORCE_INLINE_ bool operator!=(const Vector3i &a, const Vector3i &b) {
	return a.x != b.x || a.y != b.y || a.z != b.z;
}

_FORCE_INLINE_ Vector3i operator<<(const Vector3i &a, int b) {
#ifdef DEBUG_ENABLED
	CRASH_COND(b < 0);
#endif
	return Vector3i(a.x << b, a.y << b, a.z << b);
}

_FORCE_INLINE_ Vector3i operator>>(const Vector3i &a, int b) {
#ifdef DEBUG_ENABLED
	CRASH_COND(b < 0);
#endif
	return Vector3i(a.x >> b, a.y >> b, a.z >> b);
}

_FORCE_INLINE_ Vector3i operator&(const Vector3i &a, int b) {
	return Vector3i(a.x & b, a.y & b, a.z & b);
}

inline Vector3i operator%(const Vector3i &a, const Vector3i &b) {
	return Vector3i(a.x % b.x, a.y % b.y, a.z % b.z);
}

_FORCE_INLINE_ bool operator<(const Vector3i &a, const Vector3i &b) {
	if (a.x == b.x) {
		if (a.y == b.y) {
			return a.z < b.z;
		} else {
			return a.y < b.y;
		}
	} else {
		return a.x < b.x;
	}
}

_FORCE_INLINE_ int64_t Vector3i::distance_sq(const Vector3i &other) const {
	return (other - *this).length_sq();
}

#else

namespace zylann {
namespace Vector3iUtil {

constexpr int AXIS_COUNT = 3;

inline Vector3i create(int xyz) {
	return Vector3i(xyz, xyz, xyz);
}

inline void sort_min_max(Vector3i &a, Vector3i &b) {
	math::sort(a.x, b.x);
	math::sort(a.y, b.y);
	math::sort(a.z, b.z);
}

// Returning a 64-bit integer because volumes can quickly overflow INT_MAX (like 1300^3),
// even though dense volumes of that size will rarely be encountered in this module
inline int64_t get_volume(const Vector3i &v) {
#ifdef DEBUG_ENABLED
	ZN_ASSERT_RETURN_V(v.x >= 0 && v.y >= 0 && v.z >= 0, 0);
#endif
	return v.x * v.y * v.z;
}

inline unsigned int get_zxy_index(const Vector3i &v, const Vector3i area_size) {
	return v.y + area_size.y * (v.x + area_size.x * v.z); // ZXY
}

inline Vector3i from_zxy_index(unsigned int i, const Vector3i area_size) {
	Vector3i pos;
	pos.y = i % area_size.y;
	pos.x = (i / area_size.y) % area_size.x;
	pos.z = i / (area_size.y * area_size.x);
	return pos;
}

inline bool all_members_equal(const Vector3i v) {
	return v.x == v.y && v.y == v.z;
}

inline bool is_unit_vector(const Vector3i v) {
	return Math::abs(v.x) + Math::abs(v.y) + Math::abs(v.z) == 1;
}

inline bool is_valid_size(const Vector3i &s) {
	return s.x >= 0 && s.y >= 0 && s.z >= 0;
}

} // namespace Vector3iUtil

namespace math {

inline Vector3i floordiv(const Vector3i v, const Vector3i d) {
	return Vector3i(math::floordiv(v.x, d.x), math::floordiv(v.y, d.y), math::floordiv(v.z, d.z));
}

inline Vector3i floordiv(const Vector3i v, const int d) {
	return Vector3i(math::floordiv(v.x, d), math::floordiv(v.y, d), math::floordiv(v.z, d));
}

inline Vector3i ceildiv(const Vector3i v, const int d) {
	return Vector3i(math::ceildiv(v.x, d), math::ceildiv(v.y, d), math::ceildiv(v.z, d));
}

inline Vector3i ceildiv(const Vector3i v, const Vector3i d) {
	return Vector3i(math::ceildiv(v.x, d.x), math::ceildiv(v.y, d.y), math::ceildiv(v.z, d.z));
}

inline Vector3i wrap(const Vector3i v, const Vector3i d) {
	return Vector3i(math::wrap(v.x, d.x), math::wrap(v.y, d.y), math::wrap(v.z, d.z));
}

inline Vector3i clamp(const Vector3i a, const Vector3i minv, const Vector3i maxv) {
	return Vector3i(
			math::clamp(a.x, minv.x, maxv.x), math::clamp(a.y, minv.y, maxv.y), math::clamp(a.z, minv.z, maxv.z));
}

} // namespace math
} // namespace zylann

inline Vector3i operator<<(const Vector3i &a, int b) {
#ifdef DEBUG_ENABLED
	ZN_ASSERT(b >= 0);
#endif
	return Vector3i(a.x << b, a.y << b, a.z << b);
}

inline Vector3i operator>>(const Vector3i &a, int b) {
#ifdef DEBUG_ENABLED
	ZN_ASSERT(b >= 0);
#endif
	return Vector3i(a.x >> b, a.y >> b, a.z >> b);
}

inline Vector3i operator&(const Vector3i &a, uint32_t b) {
	return Vector3i(a.x & b, a.y & b, a.z & b);
}

#endif // VOXEL_CUSTOM_VECTOR3I

std::stringstream &operator<<(std::stringstream &ss, const Vector3i &v);

// For Godot
struct Vector3iHasher {
	static inline uint32_t hash(const Vector3i &v) {
		uint32_t hash = hash_djb2_one_32(v.x);
		hash = hash_djb2_one_32(v.y, hash);
		return hash_djb2_one_32(v.z, hash);
	}
};

// For STL
namespace std {
template <>
struct hash<Vector3i> {
	size_t operator()(const Vector3i &v) const {
		return Vector3iHasher::hash(v);
	}
};
} // namespace std

#endif // ZYLANN_VECTOR3I_H
